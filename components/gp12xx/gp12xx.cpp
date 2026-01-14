#include "gp12xx.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gp12xx {

static const char *const TAG = "gp12xx";

static const uint16_t GP12XX_MAX_BRIGHTNESS = 1023;

static const uint8_t GP12XX_COMMAND_SW_RESET = 0xAA;
static const uint8_t GP12XX_COMMAND_SET_VFD_MODE = 0xCC;
static const uint8_t GP12XX_COMMAND_SET_BRIGHTNESS = 0xA0;
static const uint8_t GP12XX_COMMAND_SET_DISPLAY_MODE = 0x80;
static const uint8_t GP12XX_COMMAND_SET_DISPLAY_OFFSET = 0xC0;
static const uint8_t GP12XX_COMMAND_INIT_OSC = 0x78;
static const uint8_t GP12XX_COMMAND_DISPLAY_ON = 0x6D;
static const uint8_t GP12XX_COMMAND_DISPLAY_OFF = 0x61;

void GP12XX::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");

  this->init_internal_(this->get_buffer_length_());
  this->spi_setup();

  if (this->filament_pin_ != nullptr) {
    this->filament_pin_->setup();
  }

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(1);
  }

  this->reset_();

  this->transmit_({
      GP12XX_COMMAND_SET_VFD_MODE,
      0x01,
      0x1F,
      0x00,
      0xFF,
      0x2F,
      0x00,
      0x20,
  });

  this->set_brightness(this->brightness_);

  // Clear GRAM
  this->clear();
  this->display();
  delay(20);

  this->set_invert(this->invert_);

  this->transmit_({
      GP12XX_COMMAND_SET_DISPLAY_OFFSET,
      this->offset_x_,
      this->offset_y_,
  });

  this->transmit_({
      GP12XX_COMMAND_INIT_OSC,
      0x08,
  });

  this->turn_on();
}

void GP12XX::dump_config() {
  LOG_DISPLAY("", "GP12XX", this);
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_str_());
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Filament Pin: ", this->filament_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG,
                "  Offset X: %d\n"
                "  Offset Y: %d\n"
                "  Inverted Color: %s",
                this->offset_x_, this->offset_y_, YESNO(this->invert_));
  LOG_UPDATE_INTERVAL(this);
}
void HOT GP12XX::display() {
  const int width = this->get_width_internal();
  const uint8_t height_sections = ((uint8_t) this->get_height_internal()) / 8;

  for (uint8_t y = 0; y < height_sections; y++) {
    this->enable();
    this->write_byte(0xF0);
    this->write_byte(0x00);
    this->write_byte(y * 8);
    this->write_byte(0x07);

    // Write a complete row at once for better performance
    const size_t offset = y * width;
    this->write_array(&this->buffer_[offset], width);

    this->disable();
    // Feed watchdog once per row instead of per pixel
    App.feed_wdt();
  }
}

void GP12XX::update() {
  this->do_update_();
  this->display();
}

void GP12XX::set_brightness(float brightness) {
  this->brightness_ = clamp(brightness, 0.0F, 1.0F);
  uint16_t value = uint16_t(GP12XX_MAX_BRIGHTNESS * this->brightness_);  // scale to 0-1023

  this->transmit_({
      GP12XX_COMMAND_SET_BRIGHTNESS,
      (uint8_t) (value & 0xFF),
      (uint8_t) (value >> 8),
  });
}

void GP12XX::set_invert(bool invert) {
  this->invert_ = invert;
  this->transmit_({
      GP12XX_COMMAND_SET_DISPLAY_MODE,
      (uint8_t) (this->invert_ ? 0x01 : 0x00),
  });
}

void GP12XX::turn_on() {
  // TODO: test this
  // this->transmit_(GP12XX_COMMAND_DISPLAY_ON);
  if (this->filament_pin_ != nullptr) {
    this->filament_pin_->digital_write(true);
  }
  this->is_on_ = true;
}

void GP12XX::turn_off() {
  // TODO: test this
  // this->transmit_(GP12XX_COMMAND_DISPLAY_OFF);
  if (this->filament_pin_ != nullptr) {
    this->filament_pin_->digital_write(false);
  }
  this->is_on_ = false;
}

void GP12XX::fill(Color color) {
  const uint8_t fill = color.is_on() ? 0xFF : 0x00;
  memset(this->buffer_, fill, this->get_buffer_length_());
}

void HOT GP12XX::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || x >= this->get_width_internal() || y < 0 || y >= this->get_height_internal())
    return;

  const uint16_t pos = x + (y / 8) * this->get_width_internal();
  const uint8_t subpos = y & 0x07;
  if (color.is_on()) {
    this->buffer_[pos] |= (1 << subpos);
  } else {
    this->buffer_[pos] &= ~(1 << subpos);
  }
}

int GP12XX::get_height_internal() {
  switch (this->model_) {
    case GP1294_MODEL_256_48:
      return 48;
    case GP1287_MODEL_256_50:
      return 50;
    default:
      return 0;
  }
}

int GP12XX::get_width_internal() {
  switch (this->model_) {
    case GP1294_MODEL_256_48:
      return 256;
    case GP1287_MODEL_256_50:
      return 256;
    default:
      return 0;
  }
}

void GP12XX::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(10);
    // Wake up
    this->reset_pin_->digital_write(true);
  } else {
    // TODO: test if this is really not needed with reset pin
    this->transmit_(GP12XX_COMMAND_SW_RESET);
  }
  delay(10);
}

size_t GP12XX::get_buffer_length_() {
  return size_t(this->get_width_internal()) * (size_t(this->get_height_internal())) / 8u;
}

const char *GP12XX::model_str_() {
  switch (this->model_) {
    case GP1294_MODEL_256_48:
      return "GP1294 256x48";
    case GP1287_MODEL_256_50:
      return "GP1287 256x50";
    default:
      return "Unknown";
  }
}
}  // namespace gp12xx
}  // namespace esphome
