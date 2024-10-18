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
  this->add_(0, 24, data);      // zeros
  this->add_(1, 1, data);      
  this->add_(0, 32, data);      // zeros
  
  this->add_((this->mode != climate::CLIMATE_MODE_OFF) ? 0 : 1, data); // ON / OFF
  
  this->add_(0, 3, data);      // zeros
  
  this->reverse_add_(this->set_fan_speed_(), 2, data);
  
  this->add_(0, 9, data);      // zeros

  this->reverse_add_(this->set_temp_(), 5, data);

  this->reverse_add_(this->set_mode_(), 3, data);

  this->add_(0xA5, 8, data);   // idk
  this->add_(0, 4, data);      // zeros

  //this->reverse_add_(this->set_blades_(), 4, data);
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
  if (!(curr_state.bitmap & 0x01)) {
    this->mode = climate::CLIMATE_MODE_OFF;
  }

  this->publish_state();
  return true;
}

bool FisherClimate::parse_state_frame_(const uint8_t frame[]) {
  FisherState curr_state;



  return this->parse_state_frame_(curr_state);
}


bool FisherClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t state_frame[FISHER_STATE_FRAME_SIZE] = {};
  if (!data.expect_item(FISHER_HEADER_MARK, FISHER_HEADER_SPACE)) {
    return false;
  }
  for (uint8_t pos = 0; pos < FISHER_STATE_FRAME_SIZE; pos++) {
    uint8_t byte = 0;
    for (int8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(FISHER_BIT_MARK, FISHER_ONE_SPACE)) {
        byte |= 1 << bit;
      } else if (!data.expect_item(FISHER_BIT_MARK, FISHER_ZERO_SPACE)) {
        return false;
      }
    }
    state_frame[pos] = byte;
    if (pos == 0) {
      // frame header
      if (byte != 0x11)
        return false;
    } else if (pos == 1) {
      // frame header
      if (byte != 0xDA)
        return false;
    } else if (pos == 2) {
      // frame header
      if (byte != 0x17)
        return false;
    } else if (pos == 3) {
      // frame header
      if (byte != 0x18)
        return false;
    } else if (pos == 4) {
      // frame type
      if (byte != 0x00)
        return false;
    }
  }
  return this->parse_state_frame_(state_frame);
}

}  // namespace fisher_ir
}  // namespace esphome
