#include "esphome.h"
//#include "custom.h"

#define FW_VERSION "0.05.00"
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


float g_oven_temp_target = 0.0;
float g_oven_temp_current = 0.0;

float g_probe_temps[4];
float g_probe_targets[4];
float g_fan_speeds[4];

// How many oven_temp_current calls have we received. We really only
// care about 0 and >0. If > 0 then on a set oven_temp_target we will
// immediatly set fan speed accordingly.
int g_oven_temp_current_count = 0;

float g_fan_speed = 0.0;

// Fan speed before door open
// -1 for no saved fan speed
const float fan_speed_invalid = -1.0;
float g_saved_fan_speed = fan_speed_invalid;

// bool g_enable_probe = true;
bool g_use_probe = true;

float g_fan_speed_min = 0.5;
float g_fan_speed_max = 1.0;
const float fan_speed_off = 0.0;
// How much we bump fan speed on every
// too low oven temp.
float g_fan_speed_adjust = 0.1;
// because the internal temp updates much more frequently than
// we divide the speed adjust by the value below
// when on internal probe.
float fan_speed_internal_factor = 4.0;
inline float fanSpeedAdjust() { 
  return g_fan_speed_adjust / (g_use_probe ? fan_speed_internal_factor : 1.0 );
}
bool g_door_open = false;

bool g_haveRetainedProperties = false;

//////////////////////////////////////////////////////////////////////////
// Translation 
class TranslationUnit {
  public:
  // Unit of measure text
  virtual const char* uom_text() = 0; 
  virtual bool is_match(const char* str) { return strcmp(str, uom_text()) == 0 ? true : false; };
  virtual float native_to_uom(float native_unit) = 0;
  virtual float uom_to_native(float uom_unit) = 0;
  // virtual float convert_native_to_uom(float native_unit) { return native_to_uom(native_unit); }
  // virtual float convert_uom_to_native(float uom_unit) { return (uom_unit ) * native_to_uom(); }
};

class FahrenheitTranslationUnit: public TranslationUnit {
  public:
  virtual const char* uom_text() { return "°F"; } 
  virtual float native_to_uom(float native_unit) { return native_unit * (9.0 / 5.0) + 32.0; }
  virtual float uom_to_native(float uom_unit) { return (uom_unit - 32.0) * (5.0 / 9.0); }
} fahrenheit_xlate;

class CelsiusTranslationUnit: public TranslationUnit {
  public:
  virtual const char* uom_text() { return "°C"; } 
  virtual float native_to_uom(float native_unit) { return native_unit; }
  virtual float uom_to_native(float uom_unit) { return uom_unit; }
} celsius_xlate;

// Setup initially for fahrenheit
TranslationUnit* g_xlate = &fahrenheit_xlate;

// We prefix out mqtt messages with this prefix. The value originates in the 
// yaml layer and is passed to us in on_boot. 
std::string g_mqtt_topic_prefix; 

// MQTT topics
std::string mqttTopicStat = "/stat";
std::string mqttTopicProp = "/prop";
//std::string mqttSensorWfOverlimitStatus = "/sensor/wf/over_limit/status";

void makeMqttTopics(const std::string& prefix) {

  g_mqtt_topic_prefix = prefix;

  mqttTopicStat = prefix + mqttTopicStat;
  mqttTopicProp = prefix + mqttTopicProp;
  //mqttSensorWfOverlimitStatus = prefix + mqttSensorWfOverlimitStatus;

}

#define _countof(arr) sizeof(arr) / sizeof(arr[0])

void on_boot(const char* app) {
  // delayaction = new DelayAction<std::string>();
  // App.register_component(delayaction);
  // delayaction->set_delay(100);
  //status_led->turn
  //SetStatusLED(1.0, 1.0, 1.0);
  ESP_LOGD("main", "on_boot {"); 

  g_app = app;

  for (int i = 0; i < _countof(g_probe_temps); ++i) {
      g_probe_temps[i] = 0.0;
      g_probe_targets[i] = 0.0;
      g_fan_speeds[i] = -1;
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
void process_properties(const JsonObject& x, bool fromStat=false) {

  ESP_LOGD("main", "\n\nBegin: process_properties(arg count=%i)", x.size()); 

//   if ( x.containsKey("timezone") && x["timezone"].is<char*>()) {
//     std::string was = sntp_time->get_timezone();
//     sntp_time->set_timezone(x["timezone"]);

//      ESP_LOGD("main", "timezone: specified %s, was %s now %s",  (const char*)x["timezone"], 
//       was.c_str(), sntp_time->get_timezone().c_str()); 
//  }

  if ( x.containsKey("unit_of_measurement") && x["unit_of_measurement"].is<char*>()) {
    const char* was = g_xlate->uom_text();
    const char* uom = x["unit_of_measurement"];
    TranslationUnit* xlate = nullptr;
    if (celsius_xlate.is_match(uom)) {
      xlate = &celsius_xlate;
    } else if (fahrenheit_xlate.is_match(uom)) {
      xlate = &fahrenheit_xlate;
    }
    if (xlate && xlate != g_xlate) { 
      // Need to translate stored values:
      //  g_oven_temp_target 
      //  g_oven_temp_current
        
      g_oven_temp_target = xlate->native_to_uom(g_xlate->uom_to_native(g_oven_temp_target));
      g_oven_temp_current = xlate->native_to_uom(g_xlate->uom_to_native(g_oven_temp_current));
      g_xlate = xlate;
    }
    ESP_LOGD("main", "unit_of_measurement: specified %s, was %s, now %s", uom, was, g_xlate->uom_text()); 
  }

  if (x.containsKey("oven_temp_target") && x["oven_temp_target"].is<float>())  {
    float was = g_oven_temp_target;
    g_oven_temp_target = (float)x["oven_temp_target"];
    ESP_LOGD("main", "target oven temp: was %f, now %f", was, g_oven_temp_target); 

    // Are we getting oven_temp_current calls?
    if (g_oven_temp_current_count > 0) {
      // Yes, then we are actively adjusting fan speed, so we
      // process new target with the same oven_temp_current.
      process_oven_temp(g_oven_temp_current);
    }
  }

  // oven_temp_current is in the retained "stat" messsage but ignored here from "stat" 
  if (!fromStat && (x.containsKey("oven_temp_current") && x["oven_temp_current"].is<float>()))  {
    float was = g_oven_temp_current;
    process_oven_temp((float)x["oven_temp_current"]);
    //g_oven_temp_current = (float)x["oven_temp_current"];
    ESP_LOGD("main", "current oven temp: was %f, now %f", was, g_oven_temp_current); 
  }

  if (!fromStat && (x.containsKey("fan_speed") && x["fan_speed"].is<float>()))  {
    set_fan_speed((float)x["fan_speed"]);
  }

  if ( x.containsKey("fan_speed_adjust") && x["fan_speed_adjust"].is<float>()) {
    float was = g_fan_speed_adjust;
    g_fan_speed_adjust = (float)x["fan_speed_adjust"];
    ESP_LOGD("main", "fan_speed_adjust: was %f, now %f", was, g_fan_speed_adjust); 
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
    g_use_probe = (bool)x["use_probe"];
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

void set_fan_speed_min(float fan_speed_min) {
  float was = g_fan_speed_min;
  g_fan_speed_min = fan_speed_min;
  ESP_LOGD("main", "fan_speed_min: was %f, now %f", was, g_fan_speed_min); 

  if (g_fan_speed != 0.0 && g_fan_speed < g_fan_speed_min) {
    set_fan_speed(g_fan_speed_min);
  }
}

void set_fan_speed_max(float fan_speed_max) {
  float was = g_fan_speed_max;
  g_fan_speed_max = fan_speed_max;
  ESP_LOGD("main", "fan_speed_max: was %f, now %f", was, g_fan_speed_max); 

  if (g_fan_speed > g_fan_speed_max) {
    set_fan_speed(g_fan_speed_max);
  }
}

void set_door_open(bool door_open) {

    bool was = g_door_open;
    g_door_open = door_open;
    ESP_LOGD("main", "door open: was %i, now %i", was, g_door_open); 

    if (g_door_open && !was) {
      g_saved_fan_speed = g_fan_speed;
      set_fan_speed(0.0);
    } else if (!g_door_open && was && g_saved_fan_speed != fan_speed_invalid) {
      set_fan_speed(g_saved_fan_speed);
      g_saved_fan_speed = fan_speed_invalid;
    }

    if (was != g_door_open) {
      if (g_door_open) {
        // Blink led
      } else {
        // Unblink led
      }
      send_property("door_open");
    }
}

JsonObject& make_json(JsonObject& jo, const char* prop_name=nullptr) {

    //bool all = strcmp(prop_name, "all") == 0);
    //bool all = strcmp(prop_name, "all") == 0);

    //JsonObject& jo = global_json_buffer.createObject();

    if (prop_name == nullptr) {
      jo["fw_version"] = FW_VERSION;
    }
    if (prop_name == nullptr || strcmp(prop_name, "oven_temp_target") == 0) {
      jo["oven_temp_target"] = g_oven_temp_target;
    }
    if (prop_name == nullptr || strcmp(prop_name, "oven_temp_current") == 0) {
      jo["oven_temp_current"] = g_oven_temp_current;
    }
    if (prop_name == nullptr || strcmp(prop_name, "fan_speed") == 0) {
      jo["fan_speed"] = g_fan_speed;
    }
    if (prop_name == nullptr || strcmp(prop_name, "fan_speed_min") == 0) {
      jo["fan_speed_min"] = g_fan_speed_min;
    }
    if (prop_name == nullptr || strcmp(prop_name, "fan_speed_max") == 0) {
      jo["fan_speed_max"] = g_fan_speed_max;
    }
    if (prop_name == nullptr || strcmp(prop_name, "fan_speed_adjust") == 0) {
      jo["fan_speed_adjust"] = g_fan_speed_adjust;
    }
    if (prop_name == nullptr || strcmp(prop_name, "door_open") == 0) {
      jo["door_open"] = g_door_open;
    }
    if (prop_name == nullptr || strcmp(prop_name, "unit_of_measurement") == 0) {
      jo["unit_of_measurement"] = g_xlate->uom_text();
    }
    if (prop_name == nullptr) {
      jo["use_probe"] = g_use_probe;
    }

    if (prop_name == nullptr || strcmp(prop_name, "probes") == 0) {

        // Closed periods go into array
        JsonArray& ja = global_json_buffer.createArray();

        // T* last = nullptr;
        // while ((last = getPreviousClosed(last))) {
        //     ja.add(last->toJson());
        // } 
        for (int i = 0; i < _countof(g_probe_temps); ++i) {
          JsonObject& joProbe = global_json_buffer.createObject();
          joProbe["temp"] = g_probe_temps[i];
          joProbe["target"] = g_probe_targets[i];
          ja.add(joProbe);
            //g_probe_temps[i] = 0.0;
            // g_probe_targets[i] = 0.0;
            // g_fan_speeds[i] = -1;
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

void send_properties() {

  ESP_LOGD("main", "sending stat"); 

  //JsonObject& jo = make_json();
  // Send retained message
  //mqtt_client->publish_json(mqttTopicStat, jo, 1, true);
  mqtt_client->publish_json(mqttTopicStat, [=](JsonObject &root) {
    //root = jo;
    make_json(root);
  }, 1, true);
  // mqtt_client->publish_json(mqttTopicStat, [=](JsonObject &root) {
  //   // root["timezone"] = sntp_time->get_timezone();
  //   root["fw_version"] = FW_VERSION;
  //   root["oven_temp_target"] = g_oven_temp_target;
  //   root["oven_temp_current"] = g_oven_temp_current;
  //   root["fan_speed"] = g_fan_speed;
  //   root["fan_speed_min"] = g_fan_speed_min;
  //   root["fan_speed_max"] = g_fan_speed_max;
  //   root["fan_speed_adjust"] = g_fan_speed_adjust;
  //   root["door_open"] = g_door_open;
  //   root["unit_of_measurement"] = g_xlate->uom_text();
  //   // root["enable_probe"] = g_enable_probe;
  //   root["use_probe"] = g_use_probe;
  // }, 1, true);

}

void send_property(const char* prop_name) {

  ESP_LOGD("main", "sending prop"); 

  // Send non-retained message
  mqtt_client->publish_json(mqttTopicProp, [=](JsonObject &root) {
    make_json(root, prop_name);
    // // if (strcmp(prop_name, "timezone") == 0) {
    // //   root["timezone"] = sntp_time->get_timezone();
    // // }
    // if (strcmp(prop_name, "oven_temp_target") == 0) {
    //   root["oven_temp_target"] = g_oven_temp_target;
    // }
    // if (strcmp(prop_name, "oven_temp_current") == 0) {
    //   root["oven_temp_current"] = g_oven_temp_current;
    // }
    // if (strcmp(prop_name, "fan_speed") == 0) {
    //   root["fan_speed"] = g_fan_speed;
    // }
    // if (strcmp(prop_name, "fan_speed_min") == 0) {
    //   root["fan_speed_min"] = g_fan_speed_min;
    // }
    // if (strcmp(prop_name, "fan_speed_max") == 0) {
    //   root["fan_speed_max"] = g_fan_speed_max;
    // }
    // if (strcmp(prop_name, "fan_speed_adjust") == 0) {
    //   root["fan_speed_adjust"] = g_fan_speed_adjust;
    // }
    // if (strcmp(prop_name, "door_open") == 0) {
    //   root["door_open"] = g_door_open;
    // }
    // if (strcmp(prop_name, "unit_of_measurement") == 0) {
    //   root["unit_of_measurement"] = g_xlate->uom_text();
    // }
  }, 1, false);

}

//char* fmt_display_line(char* to, float temp, float target, float fan_speed=-1);
char* fmt_display_line(char* to, int iProbe) {
  const char fmtCurTemp[] =     "P%1i:%4.0f"; 
  const char fmtTargetrTemp[] = "T:%4.0f"; 
  const char fmtFan[] =         "F:%3.0f"; 

  float temp = g_probe_temps[iProbe];
  float target = g_probe_targets[iProbe];
  float fan_speed = g_fan_speeds[iProbe];

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

void refresh_display() {
    char buf[100];

    lcd->set_writer([=](lcd_pcf8574::PCF8574LCDDisplay & it) -> void {
        it.print(0, 0, fmt_display_line((char*)buf, 0));
        it.print(0, 1, fmt_display_line((char*)buf, 1));
    });

}
// Strategy:
//
//  current < target
//    g_fan_speed += g_fan_speed_adjust

//  current > target
//    g_fan_speed = 0.0

void process_temp_received(int iProbe, float temp, bool external) {
    if ((!external && g_use_probe && iProbe == 0) || (external && !g_use_probe)) {
      process_oven_temp(temp);
    }  else {
      g_probe_temps[iProbe] = temp;
    }

    refresh_display();

}



void process_oven_temp(float oven_temp_current) {

  if (!g_door_open) {
    ++g_oven_temp_current_count;

    float was = g_oven_temp_current;
    g_oven_temp_current = oven_temp_current;

    ESP_LOGD("main", "process_oven_temp: temp_cur was=%f, now=%f, oven_temp_target=%f, fan_speed=%f", was, g_oven_temp_current, g_oven_temp_target, g_fan_speed); 

    //if (g_use_probe && was != g_oven_temp_current) {
      send_property("oven_temp_current");
    //}
    float fan_speed = g_fan_speed;
    if (g_oven_temp_current < g_oven_temp_target) {
      fan_speed = max(min(fan_speed + fanSpeedAdjust(), g_fan_speed_max), g_fan_speed_min);
    } else if (g_oven_temp_current > g_oven_temp_target) {
      fan_speed = fan_speed_off;
    }


    if (fan_speed != g_fan_speed) {
      set_fan_speed(fan_speed);
    } else {
      ESP_LOGD("main", "fan speed unchanged"); 
    }



  } else {
      ESP_LOGD("main", "process_oven_temp(%f) ignored because \"door open\"", oven_temp_current); 
  }

  g_probe_temps[0] = g_oven_temp_current;
  g_probe_targets[0] = g_oven_temp_target;
  g_fan_speeds[0] = g_fan_speed;

}

void set_fan_speed(float fan_speed) {

  float was = g_fan_speed;
  speed_pin->set_level(fan_speed);
  g_fan_speed = fan_speed;

  ESP_LOGD("main", "set_fan_speed: was %f, now %f", was, g_fan_speed); 

  if (was != g_fan_speed) {
    send_property("fan_speed");
  }


}

void process_stat(const JsonObject& x) {
  const char* job;
  if (g_haveRetainedProperties) {
    job = "ignored";
  } else {
    job = "processed";
  }
  ESP_LOGD("main", "/stat received -- %s", job);

  if (!g_haveRetainedProperties) {
    g_haveRetainedProperties = true;
    process_properties(x, true);
  }
  
 
}

float adjust_raw_temp(float temp) {
  return temp * (9.0/5.0) + 32.0;
}

void reset() {
  set_fan_speed(0.0);
  g_oven_temp_current_count = 0;
  g_saved_fan_speed = fan_speed_invalid;
}