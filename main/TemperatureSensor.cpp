#include "TemperatureSensor.hpp"

#include <ds18x20.h>

constexpr char MAX_SENSORS = 1;

TemperatureSensor::TemperatureSensor(const int gpio_pin)
    : gpio(static_cast<gpio_num_t>(gpio_pin)), sensor_addr(0) {}

esp_err_t TemperatureSensor::init() {
  esp_err_t err = gpio_set_pull_mode(gpio, GPIO_PULLUP_ONLY);
  if (err != ESP_OK) {
    return err;
  }

  onewire_addr_t addrs[MAX_SENSORS];
  size_t sensor_count;
  err = ds18x20_scan_devices(gpio, addrs, MAX_SENSORS, &sensor_count);
  if (err != ESP_OK) {
    return err;
  }

  if (sensor_count == 0) {
    return ESP_ERR_NOT_FOUND;
  }

  sensor_addr = addrs[0];
  return ESP_OK;
}

float TemperatureSensor::read() {
  if (sensor_addr == 0) {
    printf("Error: TemperatureSensor address not found for gpio %d\n", gpio);
    return 0;
  }

  float temperature;
  esp_err_t err = ds18x20_measure_and_read(gpio, sensor_addr, &temperature);
  if (err != ESP_OK) {
    printf("Error reading from TemperatureSensor on gpio %d: %s\n", gpio,
           esp_err_to_name(err));
    return 0;
  }

  return temperature;
}
