#pragma once

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace gp12xx {

enum GP12XXModel {
  GP1294_MODEL_256_48 = 0,
  GP1287_MODEL_256_50 = 1,
};

class GP12XX : public display::DisplayBuffer,
               public spi::SPIDevice<spi::BIT_ORDER_LSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                     spi::DATA_RATE_4MHZ> {
 public:
  void setup() override;
  void dump_config() override;

  void display();
  void update() override;

  float get_brightness() const { return this->brightness_; }
  void set_brightness(float brightness);
  void set_invert(bool invert);
  bool is_inverted() const { return this->invert_; }
  bool is_on() const { return this->is_on_; }
  void turn_on();
  void turn_off();

  void fill(Color color) override;

  void set_model(GP12XXModel model) { this->model_ = model; }
  void set_filament_pin(GPIOPin *filament_pin) { this->filament_pin_ = filament_pin; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }

  void init_brightness(float brightness) { this->brightness_ = brightness; }
  void init_offset_x(uint8_t offset_x) { this->offset_x_ = offset_x; }
  void init_offset_y(uint8_t offset_y) { this->offset_y_ = offset_y; }
  void init_invert(bool invert) { this->invert_ = invert; }

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  int get_height_internal() override;
  int get_width_internal() override;

  void transmit_(uint8_t value) {
    this->enable();
    this->write_byte(value);
    this->disable();
  }

  template<size_t N> void transmit_(const uint8_t (&data)[N]) {
    this->enable();
    this->write_array(data, N);
    this->disable();
  }

  void reset_();

  size_t get_buffer_length_();

  const char *model_str_();
  GP12XXModel model_{GP1294_MODEL_256_48};
  GPIOPin *filament_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  bool is_on_{false};
  float brightness_{1.0};
  uint8_t offset_x_{0};
  uint8_t offset_y_{0};
  bool invert_{false};
};

}  // namespace gp12xx
}  // namespace esphome
