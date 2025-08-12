#ifndef TEMPERATURE_SENSOR_HPP
#define TEMPERATURE_SENSOR_HPP

#include <cstdint>

#include "driver/gpio.h"

class TemperatureSensor {
 public:
  TemperatureSensor(const int gpio_pin);
  esp_err_t init();

  float read();

 private:
  const gpio_num_t gpio;
  uint64_t sensor_addr;
};

#endif
