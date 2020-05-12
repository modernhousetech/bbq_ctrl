#pragma once
#include "esphome.h"
using namespace esphome;

using temp_sensor_callback_t = std::function<void(JsonObject &)>;



class TemperatureSensor: public max6675::MAX6675Sensor {
  int id_;
  bool is_valid_ = false;

  std::function<void(int id, bool isValid)> funct_valid_state_change_;

  public:
  void update() override;

  void init(const std::string& name, int id, int pin,
    std::function<void(int id, bool isValid)>f);

  void test(std::function<void(int id, bool isValid)>f);
  void set_validity(bool is_valid);

  bool is_valid() const { return is_valid_; }

  protected:
  void read_data_();
  float adjust_raw_temp(float temp) const;


};


