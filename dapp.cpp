#include "esphome.h"
#include "dapp.h"

#define FW_VERSION "0.06.10"

using namespace esphome;
//using namespace time;
using namespace mqtt;
using namespace json;
using namespace output;

#ifdef ESP32
extern ledc::LEDCOutput *speed_pin;
#else
extern esp8266_pwm::ESP8266PWM *speed_pin;
#endif

extern mqtt::MQTTClientComponent *mqtt_client;
//extern homeassistant::HomeassistantTime *sntp_time;
extern lcd_pcf8574::PCF8574LCDDisplay *lcd;


// These are the sensor objects from esphome. We map these to our
// array: temperature_sensors_
extern sensor::Sensor *temperature_sensor0;
extern sensor::Sensor *temperature_sensor1;
extern sensor::Sensor *temperature_sensor2;
extern sensor::Sensor *temperature_sensor4;


// Our single app
dApp dapp;


void dApp::makeMqttTopics(const std::string& prefix) {

  mqtt_topic_prefix_ = prefix;

  mqttTopicStat_ = prefix + mqttTopicStat_;
  mqttTopicProp_ = prefix + mqttTopicProp_;
  
}


dApp::dApp() {
}

/*static */void dApp::on_probe_validity_change(int id, bool is_valid) {
  if (!is_valid)  {
    dapp.on_probe_validity_change_(id, is_valid);
  }
}

void dApp::on_probe_validity_change_(int id, bool is_valid) {
  refresh_display();
}


void dApp::on_boot(const char* app, int lcd_cols, int lcd_rows, const char* pinProbe0, const char* pinProbe1) {

  //SetStatusLED(1.0, 1.0, 1.0);
  ESP_LOGD("main", "on_boot {"); 

  app_ = app;

  lcd_cols_ = lcd_cols;
  lcd_rows_ = lcd_rows;

  temperature_sensors_[0] = (TemperatureSensor*)temperature_sensor0;
  // Probe 0 is special -- it controls the fan
  temperature_sensors_[0]->fan_speed = 0;

  if (app_ == "bbqmax") {
    temperature_sensor_count_ = 2;
    temperature_sensors_[1] = (TemperatureSensor*)temperature_sensor1;
  } else {
    // bbqmini
    temperature_sensor_count_ = 1;
  }

  makeMqttTopics(mqtt_client->get_topic_prefix());

  std::string probeName = "probe_";
  for (int i = 0; i < temperature_sensor_count_; ++i) {

      temperature_sensors_[i]->set_validity_callback(on_probe_validity_change);
      temperature_sensors_[i]->set_id(i);

  }

  App.register_component(delayaction_);

  ESP_LOGD("main", "} on_boot"); 
}


void dApp::process_properties(const JsonObject& jo, bool fromStat) {

  ESP_LOGD("main", "\n\nBegin: process_properties(arg count=%i)", jo.size()); 

  if ( jo.containsKey("unit_of_measurement") && jo["unit_of_measurement"].is<char*>()) {
    const char* was = xlate_->uom_text();
    const char* uom = jo["unit_of_measurement"];
    TranslationUnit* xlate = nullptr;
    if (celsius_xlate_.is_match(uom)) {
      xlate = &celsius_xlate_;
    } else if (fahrenheit_xlate_.is_match(uom)) {
      xlate = &fahrenheit_xlate_;
    }
    if (xlate && xlate != xlate_) { 
      // Need to translate stored values:

      for (int i = 0; i < temperature_sensor_count_; ++i) {
        if (temperature_sensors_[i]->target != std::numeric_limits<float>::max()) {
          temperature_sensors_[i]->target = 
            xlate->native_to_uom(xlate_->uom_to_native(temperature_sensors_[i]->target));
        }
        if (temperature_sensors_[i]->temperature != std::numeric_limits<float>::max()) {
          temperature_sensors_[i]->temperature = 
            xlate->native_to_uom(xlate_->uom_to_native(temperature_sensors_[i]->temperature));
        }
      }
      xlate_ = xlate;
    }
    ESP_LOGD("main", "unit_of_measurement: specified %s, was %s, now %s", uom, was, xlate_->uom_text()); 
  }

  if (jo.containsKey("oven_temp_target") && jo["oven_temp_target"].is<float>())  {
    float was = temperature_sensors_[0]->target;
    temperature_sensors_[0]->target = (float)jo["oven_temp_target"];
    temperature_sensors_[0]->target = temperature_sensors_[0]->target;
    ESP_LOGD("main", "target oven temp: was %f, now %f", was, temperature_sensors_[0]->target); 

    // Are we getting oven_temp_current calls?
    if (oven_temp_current_count_ > 0) {
      // Yes, then we are actively adjusting fan speed, so we
      // process new target with the same oven_temp_current.
      process_oven_temp(temperature_sensors_[0]->temperature);
    }
  }

  // oven_temp_current is in the retained "stat" messsage but ignored here from "stat" 
  if (!fromStat && (jo.containsKey("oven_temp_current") && jo["oven_temp_current"].is<float>()))  {
    float was = temperature_sensors_[0]->temperature;
    process_oven_temp((float)jo["oven_temp_current"]);
    //temperature_sensors_[0]->temperature = (float)jo["oven_temp_current"];
    ESP_LOGD("main", "current oven temp: was %f, now %f", was, temperature_sensors_[0]->temperature); 
  }

  if (jo.containsKey("probes") && jo["probes"].is<JsonArray>()) {
    float was;
    JsonArray& ja = jo["probes"];
    for (int i = 0; i < temperature_sensor_count_; ++i) {
      if (i < ja.size()) {
        if (ja[i]["target"].is<float>()) {
          was = temperature_sensors_[i]->target;
          temperature_sensors_[i]->target = ja[i]["target"];
          ESP_LOGD("main", "probe(%i).target: was %f, now %f", i, was, temperature_sensors_[i]->target); 
        }
        if (ja[i]["fan_speed"].is<float>()) {
          temperature_sensors_[i]->fan_speed = ja[i]["fan_speed"];
        }
      }
    }

  }

  if (!fromStat && (jo.containsKey("fan_speed") && jo["fan_speed"].is<float>()))  {
    set_fan_speed((float)jo["fan_speed"]);
  }

  if ( jo.containsKey("fan_speed_adjust") && jo["fan_speed_adjust"].is<float>()) {
    float was = fan_speed_adjust_;
    fan_speed_adjust_ = (float)jo["fan_speed_adjust"];
    ESP_LOGD("main", "fan_speed_adjust: was %f, now %f", was, fan_speed_adjust_); 
  }

  if ( jo.containsKey("fan_speed_min") && jo["fan_speed_min"].is<float>()) {
    set_fan_speed_min((float)jo["fan_speed_min"]);
  }

  if ( jo.containsKey("fan_speed_max") && jo["fan_speed_max"].is<float>()) {
    set_fan_speed_max((float)jo["fan_speed_max"]);
  }

  if (!fromStat && (jo.containsKey("door_open") && jo["door_open"].is<bool>()))  {
    set_door_open((bool)jo["door_open"]);
  }

  // if (!fromStat && (jo.containsKey("enable_probe") && jo["enable_probe"].is<bool>()))  {
  //   g_enable_probe = (bool)jo["enable_probe"];
  // }

  if (!fromStat && (jo.containsKey("use_probe") && jo["use_probe"].is<bool>()))  {
    use_probe_ = (bool)jo["use_probe"];
  }

  refresh_display();

  // Immediatly send retained message
  // This serves two purposes:
  //  1. If this is from set options cmnd the call gets feedback that
  //    the option has been received.
  //  2. Since the mqtt message is "retained" it will be resent to this 
  //    device whenever it connects to mqtt, providing persistence of 
  //    these values.
  send_properties();

  ESP_LOGD("main", "\nEnd: process_properties()\n"); 

}

void dApp::set_fan_speed_min(float fan_speed_min) {
  float was = fan_speed_min_;
  fan_speed_min_ = fan_speed_min;
  ESP_LOGD("main", "fan_speed_min: was %f, now %f", was, fan_speed_min_); 

  if (temperature_sensors_[0]->fan_speed != 0.0 && temperature_sensors_[0]->fan_speed < fan_speed_min_) {
    set_fan_speed(fan_speed_min_);
  }
}

void dApp::set_fan_speed_max(float fan_speed_max) {
  float was = fan_speed_max_;
  fan_speed_max_ = fan_speed_max;
  ESP_LOGD("main", "fan_speed_max: was %f, now %f", was, fan_speed_max_); 

  if (temperature_sensors_[0]->fan_speed > fan_speed_max_) {
    set_fan_speed(fan_speed_max_);
  }
}

void dApp::set_door_open(bool door_open) {

    bool was = door_open_;
    door_open_ = door_open;
    ESP_LOGD("main", "door open: was %i, now %i", was, door_open_); 

    if (door_open_ && !was) {
      saved_fan_speed_ = temperature_sensors_[0]->fan_speed;
      set_fan_speed(0.0);
    } else if (!door_open_ && was && saved_fan_speed_ != fan_speed_invalid) {
      set_fan_speed(saved_fan_speed_);
      saved_fan_speed_ = fan_speed_invalid;
    }

    if (was != door_open_) {
      if (door_open_) {
        // Blink led
      } else {
        // Unblink led
      }
      send_property("door_open");
    }
}


JsonObject& dApp::make_json(JsonObject& jo, const char* prop_name/*=nullptr*/) {

    if (prop_name == nullptr) {
      jo["fw_version"] = FW_VERSION;
    }
    if (prop_name == nullptr || strcmp(prop_name, "oven_temp_target") == 0) {
      jo["oven_temp_target"] = temperature_sensors_[0]->target;
    }
    if (prop_name == nullptr || strcmp(prop_name, "oven_temp_current") == 0) {
      jo["oven_temp_current"] = temperature_sensors_[0]->temperature;
    }
    if (prop_name == nullptr || strcmp(prop_name, "fan_speed") == 0) {
      jo["fan_speed"] = temperature_sensors_[0]->fan_speed;
    }
    if (prop_name == nullptr || strcmp(prop_name, "fan_speed_min") == 0) {
      jo["fan_speed_min"] = fan_speed_min_;
    }
    if (prop_name == nullptr || strcmp(prop_name, "fan_speed_max") == 0) {
      jo["fan_speed_max"] = fan_speed_max_;
    }
    if (prop_name == nullptr || strcmp(prop_name, "fan_speed_adjust") == 0) {
      jo["fan_speed_adjust"] = fan_speed_adjust_;
    }
    if (prop_name == nullptr || strcmp(prop_name, "door_open") == 0) {
      jo["door_open"] = door_open_;
    }
    if (prop_name == nullptr || strcmp(prop_name, "unit_of_measurement") == 0) {
      jo["unit_of_measurement"] = xlate_->uom_text();
    }
    if (prop_name == nullptr) {
      jo["use_probe"] = use_probe_;
    }

    if (prop_name == nullptr || strcmp(prop_name, "probes") == 0) {

        JsonArray& ja = global_json_buffer.createArray();

        for (int i = 0; i < temperature_sensor_count_; ++i) {
          ja.add(temperature_sensors_[i]->toJson());
        }
        // for (const TemperatureSensor *temperature_sensor : temperature_sensors_) {
        //   ja.add(temperature_sensor->toJson());
        // }

        jo["probes"] = ja;

    }

  return jo;
}

void dApp::send_properties() {

  ESP_LOGD("main", "sending stat"); 

  // Send retained message
  mqtt_client->publish_json(mqttTopicStat_, [=](JsonObject &root) {
    make_json(root);
  }, 1, true);
}

void dApp::send_property(const char* prop_name) {

  ESP_LOGD("main", "sending prop"); 

  // Send non-retained message
  mqtt_client->publish_json(mqttTopicProp_, [=](JsonObject &root) {
    make_json(root, prop_name);
  }, 1, false);

}

void dApp::get_display_values(
  int iProbe, 
  float& temperature,
  float& target,
  float& fan_speed
  ) {
  temperature = temperature_sensors_[iProbe]->temperature;
  target = temperature_sensors_[iProbe]->target;
  fan_speed = temperature_sensors_[iProbe]->fan_speed;

  // protect our buffers
  if (temperature > 9999) { temperature = 9999; }
  if (temperature < -999) { temperature = -999; }
  if (target > 9999) { target = 9999; }
  if (target < -999) { target = -999; }

  if (fan_speed >= 0) {
    fan_speed *= 1000;
    if (fan_speed > 1000) { 
      fan_speed = 1000; 
    }
  } 

}

void dApp::fmt_display_mini(char* line0, char*line1) {
  // Oven:0000 T:0000
  // Fan: 000
  const char fmtCurTemp[] =     "Oven:%4.0f"; 
  const char fmtTargetrTemp[] = "T:%4.0f"; 
  const char fmtFan[] =         "Fan: %3.0f"; 

  float temperature;
  float target;
  float fan_speed;

  get_display_values(0, temperature, target, fan_speed);

  static char spaces[] = "                              ";
  strcpy(line0, spaces);
  strcpy(line1, spaces);

  int len;

  // P1:1234 T:1234 F:100
  len = sprintf(line0, fmtCurTemp, temperature);
  line0[len] = ' ';
  sprintf(line0 + 10, fmtTargetrTemp, target);

  if (fan_speed >= 0) {
    if (fan_speed == 0) {
      strcpy(line1, "Fan: OFF");
    } else if (fan_speed >= 1000) {
      strcpy(line1, "Fan: MAX");
    } else {
      sprintf(line1, fmtFan, fan_speed);
    }
  }
  

}

char* dApp::fmt_display_line(char* to, int iProbe) {
  // 01234567890123456789
  // P1:0000 T:0000 F:000

  // Oven:0000 T:0000
  // Fan: 000
  try {
  const char fmtCurTemp[] =     "P%1i:%4.0f"; 
  const char fmtTargetrTemp[] = "T:%4.0f"; 
  const char fmtFan[] =         "F:%3.0f"; 

  float temperature;
  float target;
  float fan_speed;

  get_display_values(iProbe, temperature, target, fan_speed);

  static char spaces[] = "                              ";
  strcpy(to, spaces);

  int len;

  // P1:1234 T:1234 F:100
  len = sprintf(to, fmtCurTemp, iProbe + 1, temperature);
  to[len] = ' ';
  len = sprintf(to + 8, fmtTargetrTemp, target);

  if (fan_speed >= 0) {
    to[len + 8] = ' ';
    if (fan_speed == 0) {
      strcpy(to + 15, "F:OFF");
    } else if (fan_speed >= 1000) {
      strcpy(to + 15, "F:MAX");
    } else {
      sprintf(to + 15, fmtFan, fan_speed);
    }
  }
  return to;
  } catch(...) {
    ESP_LOGD("main", "\n\nEXCEPTION\n\n"); 
  }
}

void dApp::refresh_display() {
    char buf1[50];
    char buf2[50];

  lcd->set_writer([=](lcd_pcf8574::PCF8574LCDDisplay & it) -> void {
    if (app_ == "bbqmini") {
        fmt_display_mini((char*)buf1, (char*)buf2);  
        it.print(0, 0, buf1);
        it.print(0, 1, buf2);
    } else {
        //it.print(0, 0, fmt_display_line((char*)buf1, 0));
        for (int i = 0; i < temperature_sensor_count_; ++i) {
          if (true || temperature_sensors_[i]->is_valid()) {
            it.print(0, i, fmt_display_line((char*)buf1, i));
          }
        }
     }
   });

}
// Strategy:
//
//  current < target
//    temperature_sensors_[0]->fan_speed += fan_speed_adjust_

//  current > target
//    temperature_sensors_[0]->fan_speed = 0.0

void dApp::process_temp_received(int iProbe, float temp, bool external) {
    if ((!external && use_probe_ && iProbe == 0) || (external && !use_probe_)) {
      process_oven_temp(temp);
    }  else {
      temperature_sensors_[iProbe]->temperature = temp;
    }

    refresh_display();

}



void dApp::process_oven_temp(float oven_temp_current) {

  if (!door_open_) {
    ++oven_temp_current_count_;

    float was = temperature_sensors_[0]->temperature;
    temperature_sensors_[0]->temperature = oven_temp_current;

    ESP_LOGD("main", "process_oven_temp: temp_cur was=%f, now=%f, oven_temp_target=%f, fan_speed=%f", was, temperature_sensors_[0]->temperature, temperature_sensors_[0]->target, temperature_sensors_[0]->fan_speed); 

    //if (use_probe_ && was != temperature_sensors_[0]->temperature) {
      send_property("oven_temp_current");
    //}
    float fan_speed = temperature_sensors_[0]->fan_speed;
    if (temperature_sensors_[0]->temperature < temperature_sensors_[0]->target) {
      fan_speed = max(min(fan_speed + fanSpeedAdjust(), fan_speed_max_), fan_speed_min_);
    } else if (temperature_sensors_[0]->temperature > temperature_sensors_[0]->target) {
      fan_speed = fan_speed_off;
    }


    if (fan_speed != temperature_sensors_[0]->fan_speed) {
      set_fan_speed(fan_speed);
    } else {
      ESP_LOGD("main", "fan speed unchanged"); 
    }



  } else {
      ESP_LOGD("main", "process_oven_temp(%f) ignored because \"door open\"", oven_temp_current); 
  }

  //temperature_sensors_[0]->temperature = oven_temp_current_;
  //temperature_sensors_[0]->target = oven_temp_target_;
  //temperature_sensors_[0]->fan_speed = fan_speed_;
}

void dApp::set_fan_speed(float fan_speed) {


  // if (fan_speed > 0 && !fan_initialized_) {
  //   ESP_LOGD("main", "fan_initialized_ delay start"); 
  //   fan_initialized_ = true;
  //   speed_pin->set_level(1.0);
  //   delayaction_->set_delay(2000);
  //   delayaction_->play_complex();
  //   ESP_LOGD("main", "fan_initialized_ delay end"); 
  // }

  float was = temperature_sensors_[0]->fan_speed;
  speed_pin->set_level(fan_speed);
  temperature_sensors_[0]->fan_speed = fan_speed;

  ESP_LOGD("main", "set_fan_speed: was %f, now %f", was, temperature_sensors_[0]->fan_speed); 

  if (was != temperature_sensors_[0]->fan_speed) {
    send_property("fan_speed");
  }
}

void dApp::process_stat(const JsonObject& x) {
  const char* job;
  if (haveRetainedProperties_) {
    job = "ignored";
  } else {
    job = "processed";
  }
  ESP_LOGD("main", "/stat received -- %s", job);

  if (!haveRetainedProperties_) {
    haveRetainedProperties_ = true;
    process_properties(x, true);
  }
  
 
}

float dApp::adjust_raw_temp(float temp) {
  return temp * (9.0/5.0) + 32.0;
}

void dApp::reset() {
  set_fan_speed(0.0);
  oven_temp_current_count_ = 0;
  saved_fan_speed_ = fan_speed_invalid;
}

// JsonObject& dApp::Probe::toJson() const {
//     JsonObject& jo = global_json_buffer.createObject();
//     jo["temperature"] = temperature;
//     jo["target"] = target;
//     if (fan_speed != -1) {
//       jo["fan_speed"] = fan_speed;
//     }
//     return jo;
// }
