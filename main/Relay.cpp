#include "Relay.hpp"

Relay::Relay(const int gpio_pin) : gpio(static_cast<gpio_num_t>(gpio_pin)) {}

esp_err_t Relay::init() {
  gpio_config_t config = {};
  config.mode = GPIO_MODE_OUTPUT;
  config.pin_bit_mask = 1ULL << gpio;

  esp_err_t err = gpio_config(&config);
  if (err != ESP_OK) {
    return err;
  }

  return ESP_OK;
}

esp_err_t Relay::on() {
  esp_err_t err = gpio_set_level(gpio, 1);
  if (err != ESP_OK) {
    return err;
  }

  return ESP_OK;
}
esp_err_t Relay::off() {
  esp_err_t err = gpio_set_level(gpio, 0);
  if (err != ESP_OK) {
    return err;
  }

  return ESP_OK;
}
