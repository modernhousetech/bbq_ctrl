#pragma once
#include "esphome.h"
using namespace esphome;

using temp_sensor_callback_t = std::function<void(JsonObject &)>;



class TemperatureSensor: public max6675::MAX6675Sensor {
  int id_ = 0;
  int pin_;
  //bool is_valid_ = true;

  std::function<void(int id, bool isValid)> funct_valid_state_change_;


  public:
  static constexpr float invalid_temperature = std::numeric_limits<float>::max();
  float temperature = invalid_temperature;
  float target = invalid_temperature;
  float fan_speed = -1;

  // Now I wish I'd used getter/setters
  void set_temperature(float _temperature) {
    float was = temperature;
    temperature = _temperature;
    if (funct_valid_state_change_ != nullptr && temperature != was && 
     (temperature == invalid_temperature || 
      was == invalid_temperature)) {
        funct_valid_state_change_(id_, is_valid());
    }
  }

  // Experimenting with new (for me) c++ 11 getter/setter syntax
  // usage is: tu.calibrate_factor()
  //            tu.calibrate_factor() = 12.3
  // float temperature_ = invalid_temperature;
  // auto temperaturex()       -> float&       { return temperature_; }
  // auto temperaturex() const -> const float& { return temperature_; }


  TemperatureSensor(int pin) {
    pin_ = pin;
  }
  void update() override;
  void setup() override; 

  void set_validity_callback(std::function<void(int id, bool isValid)>f) {
    funct_valid_state_change_ = f;
  }

  void set_id(int id) { id_ = id; }

  // void test(std::function<void(int id, bool isValid)>f);
  //void set_validity(bool is_valid);

  bool is_valid() const { return temperature != invalid_temperature; }

  JsonObject& toJson() const;

  protected:
  void read_data_();
  //float adjust_raw_temp(float temp) const;


};


