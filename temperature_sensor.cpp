#include "esphome.h"
#include "temperature_sensor.h"
using namespace esphome;


sensor::SensorStateTrigger *sensor_sensorstatetrigger_msen;
Automation<float> *automation_msen;
LambdaAction<float> *lambdaaction_msen;

extern spi::SPIComponent *spi_spicomponent;

void TemperatureSensor::test(std::function<void(int iProbe, bool isValid)>f) {
  funct_valid_state_change_ = f;
}
void TemperatureSensor::set_validity(bool is_valid) {

  if (is_valid != is_valid_) {
    is_valid_ = is_valid;
    //funct_valid_state_change_(id_, is_valid_);
  }

}
// // Binding:
// std::lower_bounds(first, last, value, std::bind(&Foo::comparitor, this, _1, _2));
// // Lambda:
// std::lower_bounds(first, last, value, [](const Bar & first, const Bar & second) { return ...; });

void TemperatureSensor::init(const std::string& name, int id, int pin, 
  std::function<void(int id, bool isValid)>f) {
  ESP_LOGD("main", "'TemperatureSensor::init");

  id_ = id;
  funct_valid_state_change_ = f;

  set_spi_parent(spi_spicomponent);


  set_update_interval(15000);

  set_cs_pin(new GPIOPin(pin, OUTPUT, false));
  //App.register_sensor(this);
  set_name(name);
  set_unit_of_measurement("\302\260F");
  set_icon("mdi:thermometer");
  set_accuracy_decimals(1);
  set_force_update(false);
//   sensor::LambdaFilter* sensor_lambdafilter_msen = new sensor::LambdaFilter([=](float x) -> optional<float> {
//       return this->adjust_raw_temp((float)x);
//   });

//   set_filters({sensor_lambdafilter_msen});

//   sensor_sensorstatetrigger_msen = new sensor::SensorStateTrigger(this);
//   automation_msen = new Automation<float>(sensor_sensorstatetrigger_msen);

//   lambdaaction_msen = new LambdaAction<float>([=](float x) -> void {
//       f(1, (float)x, false);
//   });

//   automation_msen->add_actions({lambdaaction_msen});

}
/*
void TemperatureSensor::init(const std::string& name, int id, int pin, 
  std::function<void(int id, bool isValid)>f) {

  id_ = id;
  funct_valid_state_change_ = f;


  set_update_interval(15000);
  App.register_component(this);
  //set_spi_parent(spi_spicomponent);

  set_cs_pin(new GPIOPin(pin, OUTPUT, false));
  App.register_sensor(this);
  set_name(name);
  set_unit_of_measurement("\302\260F");
  set_icon("mdi:thermometer");
  set_accuracy_decimals(1);
  set_force_update(false);
//   sensor::LambdaFilter* sensor_lambdafilter_msen = new sensor::LambdaFilter([=](float x) -> optional<float> {
//       return this->adjust_raw_temp((float)x);
//   });

//   set_filters({sensor_lambdafilter_msen});

//   sensor_sensorstatetrigger_msen = new sensor::SensorStateTrigger(this);
//   automation_msen = new Automation<float>(sensor_sensorstatetrigger_msen);

//   lambdaaction_msen = new LambdaAction<float>([=](float x) -> void {
//       f(1, (float)x, false);
//   });

//   automation_msen->add_actions({lambdaaction_msen});

}
*/

void TemperatureSensor::read_data_() {
  ESP_LOGD("main", "TemperatureSensor::read_data_"); 

  this->enable();
  delay(1);
  uint8_t data[2];
  this->read_array(data, 2);
  uint16_t val = data[1] | (uint16_t(data[0]) << 8);
  this->disable();

  if ((val & 0x04) != 0) {
    // Thermocouple open
    ESP_LOGW("main", "Got invalid value from TemperatureSensor (0x%04X)", val);
    set_validity(false);
    this->status_set_warning();
    return;
  }

  set_validity(true);

  float temperature = float(val >> 3) / 4.0f;
  ESP_LOGD("main", "'%s': Got temperature=%.1f°C", this->name_.c_str(), temperature);
  this->publish_state(temperature);
  this->status_clear_warning();


  //MAX6675Sensor::read_data_();
}

void TemperatureSensor::update() {
  // this->enable();
  // delay(1);
  // // conversion initiated by rising edge
  // this->disable();

  // Conversion time typ: 170ms, max: 220ms
  auto f = std::bind(&TemperatureSensor::read_data_, this);
  this->set_timeout("value", 250, f);
}

float TemperatureSensor::adjust_raw_temp(float temp) const {
  return temp * (9.0/5.0) + 32.0;
}
