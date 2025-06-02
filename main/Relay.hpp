#ifndef RELAY_HPP
#define RELAY_HPP

#include "driver/gpio.h"

class Relay {
 public:
  Relay(const int gpio_pin);
  esp_err_t init();

  esp_err_t on();
  esp_err_t off();

 private:
  const gpio_num_t gpio;
};

#endif
