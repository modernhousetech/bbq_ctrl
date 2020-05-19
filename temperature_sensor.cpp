#include "esphome.h"
#include "temperature_sensor.h"
using namespace esphome;
using namespace json;


extern spi::SPIComponent *spi_spicomponent;

// void TemperatureSensor::test(std::function<void(int iProbe, bool isValid)>f) {
//   funct_valid_state_change_ = f;
// }
// void TemperatureSensor::set_validity(bool is_valid) {

//   if (is_valid != is_valid_) {
//     is_valid_ = is_valid;
//     funct_valid_state_change_(id_, is_valid_);
//   }

// }

void TemperatureSensor::setup() {

  set_spi_parent(spi_spicomponent);

  set_update_interval(15000);

  set_cs_pin(new GPIOPin(pin_, OUTPUT, false));
  //App.register_sensor(this);
  //set_name(name);
  set_unit_of_measurement("\302\260F");
  set_icon("mdi:thermometer");
  set_accuracy_decimals(1);
  set_force_update(false);
  //setup();
}


void TemperatureSensor::read_data_() {
  ESP_LOGD("main", " "); 
  ESP_LOGD("main", "TemperatureSensor::read_data_ pin %s pin: %i {", this->name_.c_str(), pin_); 

  this->enable();
  delay(1);
  uint8_t data[2];
  this->read_array(data, 2);
  uint16_t val = data[1] | (uint16_t(data[0]) << 8);
  this->disable();

  if ((val & 0x04) != 0) {
    // Thermocouple open
    ESP_LOGW("main", "Got invalid value from TemperatureSensor (0x%04X)", val);
    set_temperature(invalid_temperature);
    this->status_set_warning();
    return;
  }

  set_temperature( float(val >> 3) / 4.0f);
  ESP_LOGD("main", "'%s': Got temperature=%.1f°C", this->name_.c_str(), temperature);
  this->publish_state(temperature);
  this->status_clear_warning();


  ESP_LOGD("main", "} TemperatureSensor::read_data_ pin %s pin: %i\n", this->name_.c_str(), pin_); 
}

void TemperatureSensor::update() {

  // Conversion time typ: 170ms, max: 220ms
  auto f = std::bind(&TemperatureSensor::read_data_, this);
  this->set_timeout("value", 250, f);
}

// float TemperatureSensor::adjust_raw_temp(float temp) const {
//   return temp * (9.0/5.0) + 32.0;
// }

JsonObject& TemperatureSensor::toJson() const {
    JsonObject& jo = global_json_buffer.createObject();
    if (temperature != invalid_temperature) {
      jo["temperature"] = temperature;
    }
    if (target != invalid_temperature) {
      jo["target"] = target;
    }
    if (fan_speed != -1) {
      jo["fan_speed"] = fan_speed;
    }
    return jo;
}
