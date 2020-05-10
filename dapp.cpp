#include "esphome.h"
#include "dapp.h"

#define FW_VERSION "0.06.00"
// 0.02
//  Added FW_VERSION

// 0.01
// First workable version in action 

// The "app" is a nme. It is either "bbqmini" or "bbqmax". It is passed to us in on_boot()
std::string g_app;

//#define ESP32


using namespace esphome;
//using namespace time;
extern mqtt::MQTTClientComponent *mqtt_client;
using namespace mqtt;
using namespace json;
//extern homeassistant::HomeassistantTime *sntp_time;
using namespace output;
extern gpio::GPIOBinaryOutput *clockwise_pin;
#ifdef ESP32
extern ledc::LEDCOutput *speed_pin;
#else
extern esp8266_pwm::ESP8266PWM *speed_pin;
#endif
extern gpio::GPIOBinaryOutput *counter_clockwise_pin;
// DelayAction<std::string> *delayaction;
//extern esp8266_pwm::ESP8266PWM *pwr_led;
extern lcd_pcf8574::PCF8574LCDDisplay *lcd;

dApp dapp;


void dApp::makeMqttTopics(const std::string& prefix) {

  mqtt_topic_prefix_ = prefix;

  mqttTopicStat_ = prefix + mqttTopicStat_;
  mqttTopicProp_ = prefix + mqttTopicProp_;
  //mqttSensorWfOverlimitStatus = prefix + mqttSensorWfOverlimitStatus;

}

// #define _countof(arr) sizeof(arr) / sizeof(arr[0])

void dApp::on_boot(const char* app) {
  // delayaction = new DelayAction<std::string>();
  // App.register_component(delayaction);
  // delayaction->set_delay(100);
  //status_led->turn
  //SetStatusLED(1.0, 1.0, 1.0);
  ESP_LOGD("main", "on_boot {"); 

  g_app = app;

  for (int i = 0; i < _countof(probe_temps_); ++i) {
      probe_temps_[i] = 0.0;
      probe_targets_[i] = 0.0;
      fan_speeds_[i] = -1;
  }

  makeMqttTopics(mqtt_client->get_topic_prefix());

  //g_initialized = true;
  ESP_LOGD("main", "} on_boot"); 
}

// Local forward declarations
void send_properties();
void send_property(const char* prop_name);
void process_oven_temp(float oven_temp_current);
void set_fan_speed(float fan_speed);
void set_door_open(bool door_open);
void set_fan_speed_min(float fan_speed_min);
void set_fan_speed_max(float fan_speed_max);


//    {
//      "oven_temp_target": 225.0,
//      "oven_temp_current": 204.0,
//      "fan_speed": 0.7,
//      "fan_speed_base": 0.5,
//      "fan_speed_adjust": 0.1,
//      "door_open": false
//    }
void dApp::process_properties(const JsonObject& x, bool fromStat) {

  ESP_LOGD("main", "\n\nBegin: process_properties(arg count=%i)", x.size()); 

//   if ( x.containsKey("timezone") && x["timezone"].is<char*>()) {
//     std::string was = sntp_time->get_timezone();
//     sntp_time->set_timezone(x["timezone"]);

//      ESP_LOGD("main", "timezone: specified %s, was %s now %s",  (const char*)x["timezone"], 
//       was.c_str(), sntp_time->get_timezone().c_str()); 
//  }

  if ( x.containsKey("unit_of_measurement") && x["unit_of_measurement"].is<char*>()) {
    const char* was = xlate_->uom_text();
    const char* uom = x["unit_of_measurement"];
    TranslationUnit* xlate = nullptr;
    if (celsius_xlate_.is_match(uom)) {
      xlate = &celsius_xlate_;
    } else if (fahrenheit_xlate_.is_match(uom)) {
      xlate = &fahrenheit_xlate_;
    }
    if (xlate && xlate != xlate_) { 
      // Need to translate stored values:
      //  oven_temp_target_ 
      // oven_temp_current_
        
      oven_temp_target_ = xlate->native_to_uom(xlate_->uom_to_native(oven_temp_target_));
      oven_temp_current_ = xlate->native_to_uom(xlate_->uom_to_native(oven_temp_current_));
      xlate_ = xlate;
    }
    ESP_LOGD("main", "unit_of_measurement: specified %s, was %s, now %s", uom, was, xlate_->uom_text()); 
  }

  if (x.containsKey("oven_temp_target") && x["oven_temp_target"].is<float>())  {
    float was = oven_temp_target_;
    oven_temp_target_ = (float)x["oven_temp_target"];
    ESP_LOGD("main", "target oven temp: was %f, now %f", was, oven_temp_target_); 

    // Are we getting oven_temp_current calls?
    if (oven_temp_current_count_ > 0) {
      // Yes, then we are actively adjusting fan speed, so we
      // process new target with the same oven_temp_current.
      process_oven_temp(oven_temp_current_);
    }
  }

  // oven_temp_current is in the retained "stat" messsage but ignored here from "stat" 
  if (!fromStat && (x.containsKey("oven_temp_current") && x["oven_temp_current"].is<float>()))  {
    float was = oven_temp_current_;
    process_oven_temp((float)x["oven_temp_current"]);
    //oven_temp_current_ = (float)x["oven_temp_current"];
    ESP_LOGD("main", "current oven temp: was %f, now %f", was, oven_temp_current_); 
  }

  if (!fromStat && (x.containsKey("fan_speed") && x["fan_speed"].is<float>()))  {
    set_fan_speed((float)x["fan_speed"]);
  }

  if ( x.containsKey("fan_speed_adjust") && x["fan_speed_adjust"].is<float>()) {
    float was = fan_speed_adjust_;
    fan_speed_adjust_ = (float)x["fan_speed_adjust"];
    ESP_LOGD("main", "fan_speed_adjust: was %f, now %f", was, fan_speed_adjust_); 
  }

  if ( x.containsKey("fan_speed_min") && x["fan_speed_min"].is<float>()) {
    set_fan_speed_min((float)x["fan_speed_min"]);
  }

  if ( x.containsKey("fan_speed_max") && x["fan_speed_max"].is<float>()) {
    set_fan_speed_max((float)x["fan_speed_max"]);
  }

  if (!fromStat && (x.containsKey("door_open") && x["door_open"].is<bool>()))  {
    set_door_open((bool)x["door_open"]);
  }

  // if (!fromStat && (x.containsKey("enable_probe") && x["enable_probe"].is<bool>()))  {
  //   g_enable_probe = (bool)x["enable_probe"];
  // }

  if (!fromStat && (x.containsKey("use_probe") && x["use_probe"].is<bool>()))  {
    use_probe_ = (bool)x["use_probe"];
  }

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

  if (fan_speed_ != 0.0 && fan_speed_ < fan_speed_min_) {
    set_fan_speed(fan_speed_min_);
  }
}

void dApp::set_fan_speed_max(float fan_speed_max) {
  float was = fan_speed_max_;
  fan_speed_max_ = fan_speed_max;
  ESP_LOGD("main", "fan_speed_max: was %f, now %f", was, fan_speed_max_); 

  if (fan_speed_ > fan_speed_max_) {
    set_fan_speed(fan_speed_max_);
  }
}

void dApp::set_door_open(bool door_open) {

    bool was = door_open_;
    door_open_ = door_open;
    ESP_LOGD("main", "door open: was %i, now %i", was, door_open_); 

    if (door_open_ && !was) {
      saved_fan_speed_ = fan_speed_;
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

    //bool all = strcmp(prop_name, "all") == 0);
    //bool all = strcmp(prop_name, "all") == 0);

    //JsonObject& jo = global_json_buffer.createObject();

    if (prop_name == nullptr) {
      jo["fw_version"] = FW_VERSION;
    }
    if (prop_name == nullptr || strcmp(prop_name, "oven_temp_target") == 0) {
      jo["oven_temp_target"] = oven_temp_target_;
    }
    if (prop_name == nullptr || strcmp(prop_name, "oven_temp_current") == 0) {
      jo["oven_temp_current"] = oven_temp_current_;
    }
    if (prop_name == nullptr || strcmp(prop_name, "fan_speed") == 0) {
      jo["fan_speed"] = fan_speed_;
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

        // Closed periods go into array
        JsonArray& ja = global_json_buffer.createArray();

        // T* last = nullptr;
        // while ((last = getPreviousClosed(last))) {
        //     ja.add(last->toJson());
        // } 
        for (int i = 0; i < _countof(probe_temps_); ++i) {
          JsonObject& joProbe = global_json_buffer.createObject();
          joProbe["temp"] = probe_temps_[i];
          joProbe["target"] = probe_targets_[i];
          ja.add(joProbe);
            //probe_temps_[i] = 0.0;
            // probe_targets_[i] = 0.0;
            // fan_speeds_[i] = -1;
            // [ 
            //   {"temp": 44,
            //    "target": 33}
            // ],
            // [ 
            //   {"temp": 55,
            //    "target": 0}
            // ]
        }

        jo["probes"] = ja;

    }

  return jo;
}

void dApp::send_properties() {

  ESP_LOGD("main", "sending stat"); 

  //JsonObject& jo = make_json();
  // Send retained message
  //mqtt_client->publish_json(mqttTopicStat_, jo, 1, true);
  mqtt_client->publish_json(mqttTopicStat_, [=](JsonObject &root) {
    //root = jo;
    make_json(root);
  }, 1, true);
  // mqtt_client->publish_json(mqttTopicStat_, [=](JsonObject &root) {
  //   // root["timezone"] = sntp_time->get_timezone();
  //   root["fw_version"] = FW_VERSION;
  //   root["oven_temp_target"] = oven_temp_target_;
  //   root["oven_temp_current"] = oven_temp_current_;
  //   root["fan_speed"] = fan_speed_;
  //   root["fan_speed_min"] = fan_speed_min_;
  //   root["fan_speed_max"] = fan_speed_max_;
  //   root["fan_speed_adjust"] = fan_speed_adjust_;
  //   root["door_open"] = door_open_;
  //   root["unit_of_measurement"] = xlate_->uom_text();
  //   // root["enable_probe"] = g_enable_probe;
  //   root["use_probe"] = use_probe_;
  // }, 1, true);

}

void dApp::send_property(const char* prop_name) {

  ESP_LOGD("main", "sending prop"); 

  // Send non-retained message
  mqtt_client->publish_json(mqttTopicProp_, [=](JsonObject &root) {
    make_json(root, prop_name);
    // // if (strcmp(prop_name, "timezone") == 0) {
    // //   root["timezone"] = sntp_time->get_timezone();
    // // }
    // if (strcmp(prop_name, "oven_temp_target") == 0) {
    //   root["oven_temp_target"] = oven_temp_target_;
    // }
    // if (strcmp(prop_name, "oven_temp_current") == 0) {
    //   root["oven_temp_current"] = oven_temp_current_;
    // }
    // if (strcmp(prop_name, "fan_speed") == 0) {
    //   root["fan_speed"] = fan_speed_;
    // }
    // if (strcmp(prop_name, "fan_speed_min") == 0) {
    //   root["fan_speed_min"] = fan_speed_min_;
    // }
    // if (strcmp(prop_name, "fan_speed_max") == 0) {
    //   root["fan_speed_max"] = fan_speed_max_;
    // }
    // if (strcmp(prop_name, "fan_speed_adjust") == 0) {
    //   root["fan_speed_adjust"] = fan_speed_adjust_;
    // }
    // if (strcmp(prop_name, "door_open") == 0) {
    //   root["door_open"] = door_open_;
    // }
    // if (strcmp(prop_name, "unit_of_measurement") == 0) {
    //   root["unit_of_measurement"] = xlate_->uom_text();
    // }
  }, 1, false);

}

//char* fmt_display_line(char* to, float temp, float target, float fan_speed=-1);
char* dApp::fmt_display_line(char* to, int iProbe) {
  const char fmtCurTemp[] =     "P%1i:%4.0f"; 
  const char fmtTargetrTemp[] = "T:%4.0f"; 
  const char fmtFan[] =         "F:%3.0f"; 

  float temp = probe_temps_[iProbe];
  float target = probe_targets_[iProbe];
  float fan_speed = fan_speeds_[iProbe];

  // protect our buffers
  if (temp > 9999) { temp = 9999; }
  if (temp < -999) { temp = -999; }
  if (target > 9999) { target = 9999; }
  if (target < -999) { target = -999; }

  static char spaces[] = "                              ";
  strcpy(to, spaces);

  //char* pstr = to;
  int len;

  // P1:1234 T:1234 F:100
  len = sprintf(to, fmtCurTemp, iProbe + 1, temp);
  to[len] = ' ';
  len = sprintf(to + 8, fmtTargetrTemp, target);

  if (fan_speed >= 0) {
    to[len + 8] = ' ';
    fan_speed *= 1000;
    if (fan_speed > 1000) { fan_speed = 1000; }
    if (fan_speed == 0) {
      strcpy(to + 15, "F:OFF");
    } else if (fan_speed >= 1000) {
      strcpy(to + 15, "F:MAX");
    } else {
      sprintf(to + 15, fmtFan, fan_speed);
    }
  }

  return to;
}

void dApp::refresh_display() {
    char buf[100];

    lcd->set_writer([=](lcd_pcf8574::PCF8574LCDDisplay & it) -> void {
        it.print(0, 0, fmt_display_line((char*)buf, 0));
        it.print(0, 1, fmt_display_line((char*)buf, 1));
    });

}
// Strategy:
//
//  current < target
//    fan_speed_ += fan_speed_adjust_

//  current > target
//    fan_speed_ = 0.0

void dApp::process_temp_received(int iProbe, float temp, bool external) {
    if ((!external && use_probe_ && iProbe == 0) || (external && !use_probe_)) {
      process_oven_temp(temp);
    }  else {
      probe_temps_[iProbe] = temp;
    }

    refresh_display();

}



void dApp::process_oven_temp(float oven_temp_current) {

  if (!door_open_) {
    ++oven_temp_current_count_;

    float was = oven_temp_current_;
    oven_temp_current_ = oven_temp_current;

    ESP_LOGD("main", "process_oven_temp: temp_cur was=%f, now=%f, oven_temp_target=%f, fan_speed=%f", was, oven_temp_current_, oven_temp_target_, fan_speed_); 

    //if (use_probe_ && was != oven_temp_current_) {
      send_property("oven_temp_current");
    //}
    float fan_speed = fan_speed_;
    if (oven_temp_current_ < oven_temp_target_) {
      fan_speed = max(min(fan_speed + fanSpeedAdjust(), fan_speed_max_), fan_speed_min_);
    } else if (oven_temp_current_ > oven_temp_target_) {
      fan_speed = fan_speed_off;
    }


    if (fan_speed != fan_speed_) {
      set_fan_speed(fan_speed);
    } else {
      ESP_LOGD("main", "fan speed unchanged"); 
    }



  } else {
      ESP_LOGD("main", "process_oven_temp(%f) ignored because \"door open\"", oven_temp_current); 
  }

  probe_temps_[0] = oven_temp_current_;
  probe_targets_[0] = oven_temp_target_;
  fan_speeds_[0] = fan_speed_;

}

void dApp::set_fan_speed(float fan_speed) {

  float was = fan_speed_;
  speed_pin->set_level(fan_speed);
  fan_speed_ = fan_speed;

  ESP_LOGD("main", "set_fan_speed: was %f, now %f", was, fan_speed_); 

  if (was != fan_speed_) {
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