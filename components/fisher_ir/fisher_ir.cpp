#include "fisher_ir.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace fisher_ir {

static const char *const TAG = "fisher_ir.climate";

// setters
uint8_t FisherClimate::set_temp_() {
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

uint8_t FisherClimate::set_blades_() {
  if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
    switch (this->blades_) {
      case FISHER_BLADES_1:
      case FISHER_BLADES_2:
      case FISHER_BLADES_HIGH:
        this->blades_ = FISHER_BLADES_HIGH;
        break;
      case FISHER_BLADES_3:
      case FISHER_BLADES_MID:
        this->blades_ = FISHER_BLADES_MID;
        break;
      case FISHER_BLADES_4:
      case FISHER_BLADES_5:
      case FISHER_BLADES_LOW:
        this->blades_ = FISHER_BLADES_LOW;
        break;
      default:
        this->blades_ = FISHER_BLADES_FULL;
        break;
    }
  } else {
    switch (this->blades_) {
      case FISHER_BLADES_1:
      case FISHER_BLADES_2:
      case FISHER_BLADES_HIGH:
        this->blades_ = FISHER_BLADES_1;
        break;
      case FISHER_BLADES_3:
      case FISHER_BLADES_MID:
        this->blades_ = FISHER_BLADES_3;
        break;
      case FISHER_BLADES_4:
      case FISHER_BLADES_5:
      case FISHER_BLADES_LOW:
        this->blades_ = FISHER_BLADES_5;
        break;
      default:
        this->blades_ = FISHER_BLADES_STOP;
        break;
    }
  }
  return this->blades_;
}

uint8_t FisherClimate::gen_checksum_() { return (this->set_temp_() + this->set_mode_() + 2) % 16; }

// getters
float FisherClimate::get_temp_(uint8_t temp) { return (float) (temp + FISHER_TEMP_MIN); }

climate::ClimateMode FisherClimate::get_mode_(uint8_t mode) {
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

  //if (this->mode != climate::CLIMATE_MODE_OFF) {
  this->reverse_add_(this->set_mode_(), 3, data);
  this->add_(1, data);
  this->reverse_add_(this->set_fan_speed_(), 2, data);
  this->add_(this->swing_mode != climate::CLIMATE_SWING_OFF, data);
  this->add_(0, data);  // sleep mode
  this->reverse_add_(this->set_temp_(), 4, data);
  this->add_(0, 8, data);      // zeros
  this->add_(0, data);         // turbo mode
  this->add_(1, data);         // light
  this->add_(1, data);         // tree icon thingy
  this->add_(0, data);         // blow mode
  this->add_(0x52, 11, data);  // idk

  this->reverse_add_(this->set_blades_(), 4, data);
  this->add_(0, 4, data);          // zeros
  this->reverse_add_(2, 2, data);  // thermometer
  this->add_(0, 18, data);         // zeros
  this->reverse_add_(this->gen_checksum_(), 4, data);

  
  data->mark(FISHER_ZERO_SPACE);
  data->space(FISHER_HEADER_SPACE);
  data->mark(FISHER_ZERO_SPACE);

  transmit.perform();
}

bool FisherClimate::parse_state_frame_(FisherState curr_state) {
  this->mode = this->get_mode_(curr_state.mode);
  this->fan_mode = this->get_fan_speed_(curr_state.fan_speed);
  this->target_temperature = this->get_temp_(curr_state.temp);
  this->swing_mode = this->get_swing_(curr_state.bitmap);
  // this->blades_ = curr_state.fan_pos;
  if (!(curr_state.bitmap & 0x01)) {
    this->mode = climate::CLIMATE_MODE_OFF;
  }

  this->publish_state();
  return true;
}

bool FisherClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (!data.expect_item(FISHER_HEADER_MARK, FISHER_HEADER_SPACE)) {
    return false;
  }
  ESP_LOGD(TAG, "Received fisher frame");

  FisherState curr_state;

  for (size_t pos = 0; pos < 3; pos++) {
    if (data.expect_item(FISHER_BIT_MARK, FISHER_ONE_SPACE)) {
      curr_state.mode |= 1 << pos;
    } else if (!data.expect_item(FISHER_BIT_MARK, FISHER_ZERO_SPACE)) {
      return false;
    }
  }

  ESP_LOGD(TAG, "Mode: %d", curr_state.mode);

  if (data.expect_item(FISHER_BIT_MARK, FISHER_ONE_SPACE)) {
    curr_state.bitmap |= 1 << 0;
  } else if (!data.expect_item(FISHER_BIT_MARK, FISHER_ZERO_SPACE)) {
    return false;
  }

  ESP_LOGD(TAG, "On: %d", curr_state.bitmap & 0x01);

  for (size_t pos = 0; pos < 2; pos++) {
    if (data.expect_item(FISHER_BIT_MARK, FISHER_ONE_SPACE)) {
      curr_state.fan_speed |= 1 << pos;
    } else if (!data.expect_item(FISHER_BIT_MARK, FISHER_ZERO_SPACE)) {
      return false;
    }
  }

  ESP_LOGD(TAG, "Fan speed: %d", curr_state.fan_speed);

  for (size_t pos = 0; pos < 2; pos++) {
    if (data.expect_item(FISHER_BIT_MARK, FISHER_ONE_SPACE)) {
      curr_state.bitmap |= 1 << (pos + 1);
    } else if (!data.expect_item(FISHER_BIT_MARK, FISHER_ZERO_SPACE)) {
      return false;
    }
  }

  ESP_LOGD(TAG, "Swing: %d", (curr_state.bitmap >> 1) & 0x01);
  ESP_LOGD(TAG, "Sleep: %d", (curr_state.bitmap >> 2) & 0x01);

  for (size_t pos = 0; pos < 4; pos++) {
    if (data.expect_item(FISHER_BIT_MARK, FISHER_ONE_SPACE)) {
      curr_state.temp |= 1 << pos;
    } else if (!data.expect_item(FISHER_BIT_MARK, FISHER_ZERO_SPACE)) {
      return false;
    }
  }

  ESP_LOGD(TAG, "Temp: %d", curr_state.temp);

  for (size_t pos = 0; pos < 8; pos++) {
    if (!data.expect_item(FISHER_BIT_MARK, FISHER_ZERO_SPACE)) {
      return false;
    }
  }

  for (size_t pos = 0; pos < 4; pos++) {
    if (data.expect_item(FISHER_BIT_MARK, FISHER_ONE_SPACE)) {
      curr_state.bitmap |= 1 << (pos + 3);
    } else if (!data.expect_item(FISHER_BIT_MARK, FISHER_ZERO_SPACE)) {
      return false;
    }
  }

  ESP_LOGD(TAG, "Turbo: %d", (curr_state.bitmap >> 3) & 0x01);
  ESP_LOGD(TAG, "Light: %d", (curr_state.bitmap >> 4) & 0x01);
  ESP_LOGD(TAG, "Tree: %d", (curr_state.bitmap >> 5) & 0x01);
  ESP_LOGD(TAG, "Blow: %d", (curr_state.bitmap >> 6) & 0x01);

  uint16_t control_data = 0;
  for (size_t pos = 0; pos < 11; pos++) {
    if (data.expect_item(FISHER_BIT_MARK, FISHER_ONE_SPACE)) {
      control_data |= 1 << pos;
    } else if (!data.expect_item(FISHER_BIT_MARK, FISHER_ZERO_SPACE)) {
      return false;
    }
  }

  if (control_data != 0x250) {
    return false;
  }

  return this->parse_state_frame_(curr_state);
}

}  // namespace fisher_ir
}  // namespace esphome
