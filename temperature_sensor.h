#pragma once
#include "esphome.h"
using namespace esphome;

using temp_sensor_callback_t = std::function<void(JsonObject &)>;



class TemperatureSensor: public max6675::MAX6675Sensor {
  int id_;
  int pin_;
  bool is_valid_ = false;

  std::function<void(int id, bool isValid)> funct_valid_state_change_;

  public:
  TemperatureSensor(const char* pin=nullptr) {
    //pin_ = pin;
  }
  void update() override; /* {
    //MAX6675Sensor::update();
  } */
  void setup() override {
    // This will be called by App.setup()
    init("probe99", 33, 33, nullptr);

  }


  void init(const std::string& name, int id, int pin,
    std::function<void(int id, bool isValid)>f);

  void test(std::function<void(int id, bool isValid)>f);
  void set_validity(bool is_valid);

  bool is_valid() const { return is_valid_; }

  protected:
  void read_data_();
  float adjust_raw_temp(float temp) const;


};


