#include <stdio.h>

#include "LoopManager.hpp"
#include "MQTTManager.hpp"
#include "Mode.hpp"
#include "OperatingState.hpp"
#include "Relay.hpp"
#include "Storage.hpp"
#include "TemperatureSensor.hpp"
#include "WiFiManager.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

// MQTT topics
constexpr const char* MQTT_CURRENT_STATE_TOPIC =
    CONFIG_MQTT_CURRENT_STATE_TOPIC;
constexpr const char* MQTT_TARGET_STATE_TOPIC = CONFIG_MQTT_TARGET_STATE_TOPIC;

WiFiManager wifi(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
MQTTManager mqtt(CONFIG_MQTT_BROKER_URL, CONFIG_MQTT_CLIENT_ID, CONFIG_MQTT_QOS,
                 CONFIG_MQTT_RETENTION_POLICY);
Storage storage(CONFIG_DEFAULT_MODE, CONFIG_DEFAULT_TARGET_TEMPERATURE);
LoopManager loop_manager(CONFIG_TEMPERATURE_CHECK_INTERVAL_MS);

TemperatureSensor temperature_sensor(CONFIG_TEMPERATURE_SENSOR_GPIO);
Relay fridge(CONFIG_FRIDGE_RELAY_GPIO);
Relay heater(CONFIG_HEATER_RELAY_GPIO);

extern "C" void app_main(void) {
  esp_err_t err = nvs_flash_init();
  if (err != ESP_OK) {
    printf("Error initializing NVS: %s\n", esp_err_to_name(err));
    esp_restart();
  }

  err = wifi.init();
  if (err != ESP_OK) {
    printf("Error initializing WiFi manager: %s\n", esp_err_to_name(err));
    esp_restart();
  }

  err = mqtt.init();
  if (err != ESP_OK) {
    printf("Error initializing MQTT client: %s\n", esp_err_to_name(err));
    esp_restart();
  }

  err = storage.init();
  if (err != ESP_OK) {
    printf("Error initializing storage: %s\n", esp_err_to_name(err));
    esp_restart();
  }

  err = temperature_sensor.init();
  if (err != ESP_OK) {
    printf("Error initializing temperature sensor: %s\n", esp_err_to_name(err));
    esp_restart();
  }

  err = fridge.init();
  if (err != ESP_OK) {
    printf("Error initializing fridge relay: %s\n", esp_err_to_name(err));
    esp_restart();
  }

  err = heater.init();
  if (err != ESP_OK) {
    printf("Error initializing heater relay: %s\n", esp_err_to_name(err));
    esp_restart();
  }

  wifi.on_connect([]() {
    esp_err_t err = mqtt.start();
    if (err != ESP_OK) {
      printf("Error starting MQTT client: %s\n", esp_err_to_name(err));
      esp_restart();
    }
  });

  wifi.on_disconnect([]() {
    esp_err_t err = mqtt.stop();
    if (err != ESP_OK) {
      printf("Error stopping MQTT client: %s\n", esp_err_to_name(err));
      esp_restart();
    }
  });

  mqtt.subscribe(MQTT_TARGET_STATE_TOPIC, [](const char* message) {
    esp_err_t err = storage.populate_from_json(message);
    if (err != ESP_OK) {
      printf("Error populating storage from JSON message '%s': %s\n", message,
             esp_err_to_name(err));
      return;
    }

    // Force run immediately to apply the changes
    loop_manager.force_run();

    Mode mode = storage.get_mode();
    int target_temperature = storage.get_target_temperature();
    printf("Set target state: mode=%s, target_temperature=%d\n",
           mode_to_str(mode), target_temperature);
  });

  OperatingState operating_state = OperatingState::IDLE;
  while (true) {
    if (loop_manager.should_run()) {
      float threshold;
      if (operating_state == OperatingState::IDLE) {
        // If we're idling, we use a bigger threshold to avoid rapid toggling
        threshold = 0.5;
      } else {
        // If we're heating or cooling, we want to reach the target temperature
        // as close as possible, so we use a smaller threshold
        threshold = 0.1;
      }

      float current_temperature = temperature_sensor.read();
      int target_temperature = storage.get_target_temperature();
      Mode mode = storage.get_mode();

      if (current_temperature > target_temperature + threshold &&
          (mode == Mode::AUTO || mode == Mode::COOL)) {
        if (fridge.on() != ESP_OK) printf("Error turning on the fridge\n");
        if (heater.off() != ESP_OK) printf("Error turning off the heater\n");
        operating_state = OperatingState::COOLING;
      } else if (current_temperature < target_temperature - threshold &&
                 (mode == Mode::AUTO || mode == Mode::HEAT)) {
        if (fridge.off() != ESP_OK) printf("Error turning off the fridge\n");
        if (heater.on() != ESP_OK) printf("Error turning on the heater\n");
        operating_state = OperatingState::HEATING;
      } else {
        if (fridge.off() != ESP_OK) printf("Error turning off the fridge\n");
        if (heater.off() != ESP_OK) printf("Error turning off the heater\n");
        operating_state = OperatingState::IDLE;
      }

      char message[72];
      snprintf(message, sizeof(message),
               "{\"currentTemperature\":%.2f,\"operatingState\":\"%s\"}",
               current_temperature, operating_state_to_str(operating_state));
      mqtt.publish(MQTT_CURRENT_STATE_TOPIC, message);
    }

    // Minimal delay to avoid busy-waiting
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
