#include "fisher_ir.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace fisher_ir {

static const char *const TAG = "fisher_ir.climate";

// setters
uint8_t FisherClimate::set_temp_() 
{
  return (uint8_t) roundf(clamp<float>(this->target_temperature, FISHER_TEMP_MIN, FISHER_TEMP_MAX) - FISHER_TEMP_MIN);
}

uint8_t FisherClimate::set_mode_() {
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      return FISHER_MODE_COOL;
    case climate::CLIMATE_MODE_DRY:
      return FISHER_MODE_DRY;
    case climate::CLIMATE_MODE_HEAT:
      return FISHER_MODE_HEAT;
    case climate::CLIMATE_MODE_FAN_ONLY:
      return FISHER_MODE_FAN;
    case climate::CLIMATE_MODE_HEAT_COOL:
    default:
      return FISHER_MODE_AUTO;
  }
}

uint8_t FisherClimate::set_fan_speed_() {
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:
      return FISHER_FAN_1;
    case climate::CLIMATE_FAN_MEDIUM:
      return FISHER_FAN_2;
    case climate::CLIMATE_FAN_HIGH:
      return FISHER_FAN_3;
    case climate::CLIMATE_FAN_AUTO:
    default:
      return FISHER_FAN_AUTO;
  }
}

uint8_t FisherClimate::gen_checksum_() { return (this->set_temp_() + this->set_mode_() + 2) % 16; }

// getters
float FisherClimate::get_temp_(uint8_t temp) { return (float) (temp + FISHER_TEMP_MIN); }

climate::ClimateMode FisherClimate::get_mode_(uint8_t on_off, uint8_t mode) 
{
  if (on_off == 0)
  {
    return climate::CLIMATE_MODE_OFF;
  }

  switch (mode) {
    case FISHER_MODE_COOL:
      return climate::CLIMATE_MODE_COOL;
    case FISHER_MODE_DRY:
      return climate::CLIMATE_MODE_DRY;
    case FISHER_MODE_HEAT:
      return climate::CLIMATE_MODE_HEAT;
    case FISHER_MODE_AUTO:
      return climate::CLIMATE_MODE_HEAT_COOL;
    case FISHER_MODE_FAN:
      return climate::CLIMATE_MODE_FAN_ONLY;
    default:
      return climate::CLIMATE_MODE_HEAT_COOL;
  }
}

climate::ClimateFanMode FisherClimate::get_fan_speed_(uint8_t fan_speed) {
  switch (fan_speed) {
    case FISHER_FAN_1:
      return climate::CLIMATE_FAN_LOW;
    case FISHER_FAN_2:
      return climate::CLIMATE_FAN_MEDIUM;
    case FISHER_FAN_3:
      return climate::CLIMATE_FAN_HIGH;
    case FISHER_FAN_AUTO:
    default:
      return climate::CLIMATE_FAN_AUTO;
  }
}

climate::ClimateSwingMode FisherClimate::get_swing_(uint8_t bitmap) {
  return (bitmap >> 1) & 0x01 ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
}

template<typename T> T FisherClimate::reverse_(T val, size_t len) {
  T result = 0;
  for (size_t i = 0; i < len; i++) {
    result |= ((val & 1 << i) != 0) << (len - 1 - i);
  }
  return result;
}

template<typename T> void FisherClimate::add_(T val, size_t len, esphome::remote_base::RemoteTransmitData *data) {
  for (size_t i = len; i > 0; i--) {
    data->mark(FISHER_BIT_MARK);
    data->space((val & (1 << (i - 1))) ? FISHER_ONE_SPACE : FISHER_ZERO_SPACE);
  }
}

template<typename T> void FisherClimate::add_(T val, esphome::remote_base::RemoteTransmitData *data) {
  data->mark(FISHER_BIT_MARK);
  data->space((val & 1) ? FISHER_ONE_SPACE : FISHER_ZERO_SPACE);
}

template<typename T>
void FisherClimate::reverse_add_(T val, size_t len, esphome::remote_base::RemoteTransmitData *data) {
  this->add_(this->reverse_(val, len), len, data);
}

bool FisherClimate::check_checksum_(uint8_t checksum) {
  uint8_t expected = this->gen_checksum_();
  ESP_LOGV(TAG, "Expected checksum: %X", expected);
  ESP_LOGV(TAG, "Checksum received: %X", checksum);

  return checksum == expected;
}

void FisherClimate::transmit_state() {
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(FISHER_IR_FREQUENCY);

  data->mark(FISHER_HEADER_MARK);
  data->space(FISHER_HEADER_SPACE);
  this->add_(0, 24, data);      // zeros
  this->add_(1, 1, data);      
  this->add_(0, 32, data);      // zeros
  
  this->add_((this->mode != climate::CLIMATE_MODE_OFF) ? 1 : 0, data); // ON / OFF
  
  this->add_(0, 3, data);      // zeros
  
  this->reverse_add_(this->set_fan_speed_(), 2, data);
  
  this->add_(0, 9, data);      // zeros

  this->reverse_add_(this->set_temp_(), 5, data);

  this->reverse_add_(this->set_mode_(), 3, data);

  this->add_(0, 8, data);      // zeros

  this->reverse_add_(this->gen_checksum_(), 8, data);

  data->mark(FISHER_ZERO_SPACE);
  data->space(FISHER_HEADER_SPACE);
  data->mark(FISHER_ZERO_SPACE);

  transmit.perform();
}

bool FisherClimate::parse_state_frame_(FisherState curr_state) 
{
  ESP_LOGI(TAG, "Parse state frame");

  this->mode = this->get_mode_(curr_state.on_off, curr_state.mode);
  this->fan_mode = this->get_fan_speed_(curr_state.fan_speed);
  this->target_temperature = this->get_temp_(curr_state.temp);
  this->swing_mode = this->get_swing_(curr_state.bitmap);
  
  this->publish_state();
  return true;
}

bool FisherClimate::on_receive(remote_base::RemoteReceiveData data)
{
  ESP_LOGI(TAG, "receive");
  FisherState curr_state;
  if (!data.expect_item(FISHER_HEADER_MARK, FISHER_HEADER_SPACE)) 
  {
    return false;
  }

  ESP_LOGI(TAG, "Received Fisher frame");


  for (size_t pos = 0; pos < 57; pos++) 
  {
    if (!data.expect_item(FISHER_BIT_MARK, FISHER_ONE_SPACE) && 
        !data.expect_item(FISHER_BIT_MARK, FISHER_ZERO_SPACE)) 
    {
      ESP_LOGI(TAG, "Wrong data 57 - %d", pos);
      return false;
    } 
  }

  if (data.expect_item(FISHER_BIT_MARK, FISHER_ONE_SPACE)) 
  {
    curr_state.on_off = 1;
  } 
  else if (!data.expect_item(FISHER_BIT_MARK, FISHER_ZERO_SPACE)) 
  {
    ESP_LOGI(TAG, "Wrong data onoff");
    return false;
  }

  ESP_LOGI(TAG, "On/Off: %d", curr_state.on_off);

  for (size_t pos = 0; pos < 3; pos++) 
  {
    if (!data.expect_item(FISHER_BIT_MARK, FISHER_ONE_SPACE) && 
        !data.expect_item(FISHER_BIT_MARK, FISHER_ZERO_SPACE)) 
    {
      ESP_LOGI(TAG, "Wrong data 3");
      return false;
    } 
  }

  for (size_t pos = 0; pos < 2; pos++) 
  {
    if (data.expect_item(FISHER_BIT_MARK, FISHER_ONE_SPACE)) 
    {
      curr_state.fan_speed |= 1 << pos;
    }
    else if (!data.expect_item(FISHER_BIT_MARK, FISHER_ZERO_SPACE)) 
    {
      ESP_LOGI(TAG, "Wrong data fan");
      return false;
    }
  }

  ESP_LOGI(TAG, "Fan speed: %d", curr_state.fan_speed);
  
  for (size_t pos = 0; pos < 9; pos++) 
  {
    if (!data.expect_item(FISHER_BIT_MARK, FISHER_ONE_SPACE) && 
        !data.expect_item(FISHER_BIT_MARK, FISHER_ZERO_SPACE)) 
    {
      return false;
    } 
  }

  for (size_t pos = 0; pos < 5; pos++) 
  {
    if (data.expect_item(FISHER_BIT_MARK, FISHER_ONE_SPACE)) 
    {
      curr_state.temp |= 1 << pos;
    } 
    else if (!data.expect_item(FISHER_BIT_MARK, FISHER_ZERO_SPACE)) 
    {
      return false;
    }
  }
  ESP_LOGI(TAG, "Temp: %d", curr_state.temp + FISHER_TEMP_MIN);

  for (size_t pos = 0; pos < 3; pos++) 
  {
    if (data.expect_item(FISHER_BIT_MARK, FISHER_ONE_SPACE)) 
    {
      curr_state.mode |= 1 << pos;
    } 
    else if (!data.expect_item(FISHER_BIT_MARK, FISHER_ZERO_SPACE)) 
    {
      return false;
    }
  }
  ESP_LOGI(TAG, "Mode: %d", curr_state.mode);

  return this->parse_state_frame_(curr_state);
}

}  // namespace fisher_ir
}  // namespace esphome
