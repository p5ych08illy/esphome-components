#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace fisher_ir {

const uint8_t FISHER_TEMP_MIN = 16;  // Celsius
const uint8_t FISHER_TEMP_MAX = 32;  // Celsius

// Modes

enum FisherMode : uint8_t {
  FISHER_MODE_AUTO = 0x00,
  FISHER_MODE_COOL = 0x01,
  FISHER_MODE_DRY = 0x02,
  FISHER_MODE_FAN = 0x03,
  FISHER_MODE_HEAT = 0x04,
};

// Fan Speed

enum FisherFanMode : uint8_t {
  FISHER_FAN_AUTO = 0x00,
  FISHER_FAN_1 = 0x03,
  FISHER_FAN_2 = 0x02,
  FISHER_FAN_3 = 0x01,
};

// IR Transmission
const uint32_t FISHER_STATE_FRAME_SIZE = 96; //?
const uint32_t FISHER_IR_FREQUENCY = 38000;
const uint32_t FISHER_HEADER_MARK = 6050;
const uint32_t FISHER_HEADER_SPACE = 7450;
const uint32_t FISHER_BIT_MARK = 500;
const uint32_t FISHER_ONE_SPACE = 1630;
const uint32_t FISHER_ZERO_SPACE = 620;

struct FisherState {
  uint8_t mode = 0;
  uint8_t bitmap = 0;
  uint8_t fan_speed = 0;
  uint8_t temp = 0;
  uint8_t on_off = 0;
  uint8_t checksum = 0;
};

class FisherClimate : public climate_ir::ClimateIR {
 public:
  FisherClimate()
      : climate_ir::ClimateIR(FISHER_TEMP_MIN, FISHER_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH}, {}) {}

 protected:
  // Transmit via IR the state of this climate controller
  void transmit_state() override;
  // Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool parse_state_frame_(FisherState curr_state);

  // setters
  uint8_t set_mode_();
  uint8_t set_temp_();
  uint8_t set_fan_speed_();
  uint8_t gen_checksum_();

  // getters
  climate::ClimateMode get_mode_(uint8_t on_off, uint8_t mode);
  climate::ClimateFanMode get_fan_speed_(uint8_t fan);
  // get swing
  climate::ClimateSwingMode get_swing_(uint8_t bitmap);
  float get_temp_(uint8_t temp);

  // check if the received frame is valid
  bool check_checksum_(uint8_t checksum);

  template<typename T> T reverse_(T val, size_t len);

  template<typename T> void add_(T val, size_t len, esphome::remote_base::RemoteTransmitData *ata);

  template<typename T> void add_(T val, esphome::remote_base::RemoteTransmitData *data);

  template<typename T> void reverse_add_(T val, size_t len, esphome::remote_base::RemoteTransmitData *data);

};

}  // namespace fisher_ir
}  // namespace esphome
