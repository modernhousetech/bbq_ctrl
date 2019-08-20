#include "esphome.h"
//#include "custom.h"

#define FW_VERSION "0.03.01"
// 0.02
//  Added FW_VERSION

// 0.01
// First workable version in action 

// Must also define in smoker_fan_ctrl.yaml
#define DEVICENAME "smoker_fan_ctrl"

using namespace esphome;
//using namespace time;
extern mqtt::MQTTClientComponent *mqtt_client;
using namespace mqtt;
using namespace json;
//extern homeassistant::HomeassistantTime *sntp_time;
using namespace output;
extern gpio::GPIOBinaryOutput *clockwise_pin;
extern esp8266_pwm::ESP8266PWM *speed_pin;
extern gpio::GPIOBinaryOutput *counter_clockwise_pin;
// DelayAction<std::string> *delayaction;
extern esp8266_pwm::ESP8266PWM *pwr_led;


float g_temp_target = 0.0;
float g_temp_current = 0.0;

// How many temp_current calls have we received. We really only
// care about 0 and >0. If > 0 then on a set temp_target we will
// immediatly set fan speed accordingly.
int g_temp_current_count = 0;

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

void on_boot() {
  // delayaction = new DelayAction<std::string>();
  // App.register_component(delayaction);
  // delayaction->set_delay(100);
  //status_led->turn
}

// Local forward declarations
void send_properties();
void send_property(const char* prop_name);
void process_oven_temp(float temp_current);
void set_fan_speed(float fan_speed);
void set_door_open(bool door_open);
void set_fan_speed_min(float fan_speed_min);
void set_fan_speed_max(float fan_speed_max);


//    {
//      "temp_target": 225.0,
//      "temp_current": 204.0,
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

  // if ( x.containsKey("unit_of_measure") && x["unit_of_measure"].is<char*>()) {
  //   const char* was = g_xlate->uom_text();
  //   const char* uom = x["unit_of_measure"];
  //   TranslationUnit* xlate = nullptr;
  //   if (liter_xlate.is_match(uom)) {
  //     xlate = &liter_xlate;
  //   } else if (gal_xlate.is_match(uom)) {
  //     xlate = &gal_xlate;
  //   }
  //   if (xlate && xlate != g_xlate) { 
  //     // Need to translate stored values
  //     g_max_wf = xlate->convert_pulses_to_uom(g_xlate->convert_uom_to_pulses(g_max_wf));
  //     g_hourlyWaterUsage.convert( [=](float &val) {
  //        return xlate->convert_pulses_to_uom(g_xlate->convert_uom_to_pulses(val));
  //     });
  //     g_xlate = xlate;
  //   }
  //   ESP_LOGD("main", "unit_of_measure: specified %s, was %s, now %s", uom, was, g_xlate->uom_text()); 
  // }

  if (x.containsKey("temp_target") && x["temp_target"].is<float>())  {
    float was = g_temp_target;
    g_temp_target = (float)x["temp_target"];
    ESP_LOGD("main", "target temp: was %f, now %f", was, g_temp_target); 

    // Are we getting temp_current calls?
    if (g_temp_current_count > 0) {
      // Yes, then we are actively adjusting fan speed, so we
      // process new target with the same temp_current.
      process_oven_temp(g_temp_current);
    }
  }

  // temp_current is in the retained "stat" messsage but ignored here from "stat" 
  if (!fromStat && (x.containsKey("temp_current") && x["temp_current"].is<float>()))  {
    float was = g_temp_current;
    process_oven_temp((float)x["temp_current"]);
    //g_temp_current = (float)x["temp_current"];
    ESP_LOGD("main", "current temp: was %f, now %f", was, g_temp_current); 
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

void send_properties() {

  ESP_LOGD("main", "sending stat"); 

  // Send retained message
  mqtt_client->publish_json(DEVICENAME "/stat", [=](JsonObject &root) {
    // root["timezone"] = sntp_time->get_timezone();
    root["fw_version"] = FW_VERSION;
    root["temp_target"] = g_temp_target;
    root["temp_current"] = g_temp_current;
    root["fan_speed"] = g_fan_speed;
    root["fan_speed_min"] = g_fan_speed_min;
    root["fan_speed_max"] = g_fan_speed_max;
    root["fan_speed_adjust"] = g_fan_speed_adjust;
    root["door_open"] = g_door_open;
    // root["unit_of_measure"] = g_xlate->uom_text();
    // root["enable_probe"] = g_enable_probe;
    root["use_probe"] = g_use_probe;
  }, 1, true);

}

void send_property(const char* prop_name) {

  ESP_LOGD("main", "sending prop"); 

  // Send non-retained message
  mqtt_client->publish_json(DEVICENAME "/prop", [=](JsonObject &root) {
    // if (strcmp(prop_name, "timezone") == 0) {
    //   root["timezone"] = sntp_time->get_timezone();
    // }
    if (strcmp(prop_name, "temp_target") == 0) {
      root["temp_target"] = g_temp_target;
    }
    if (strcmp(prop_name, "temp_current") == 0) {
      root["temp_current"] = g_temp_current;
    }
    if (strcmp(prop_name, "fan_speed") == 0) {
      root["fan_speed"] = g_fan_speed;
    }
    if (strcmp(prop_name, "fan_speed_min") == 0) {
      root["fan_speed_min"] = g_fan_speed_min;
    }
    if (strcmp(prop_name, "fan_speed_max") == 0) {
      root["fan_speed_max"] = g_fan_speed_max;
    }
    if (strcmp(prop_name, "fan_speed_adjust") == 0) {
      root["fan_speed_adjust"] = g_fan_speed_adjust;
    }
    if (strcmp(prop_name, "door_open") == 0) {
      root["door_open"] = g_door_open;
    }
    // if (strcmp(prop_name, "unit_of_measure") == 0) {
    //   root["unit_of_measure"] = g_xlate->uom_text();
    // }
  }, 1, false);

}

// Strategy:
//
//  current < target
//    g_fan_speed += g_fan_speed_adjust

//  current > target
//    g_fan_speed = 0.0

void process_temp_received(float temp, bool external) {
  if ((!external && g_use_probe) || (external && !g_use_probe)) {
    process_oven_temp(temp);
  }  
}


void process_oven_temp(float temp_current) {

  if (!g_door_open) {
    ++g_temp_current_count;

    float was = g_temp_current;
    g_temp_current = temp_current;

    ESP_LOGD("main", "process_oven_temp: temp_cur was=%f, now=%f, temp_target=%f, fan_speed=%f", was, g_temp_current, g_temp_target, g_fan_speed); 

    //if (g_use_probe && was != g_temp_current) {
      send_property("temp_current");
    //}

    float fan_speed = g_fan_speed;
    if (g_temp_current < g_temp_target) {
      fan_speed = max(min(fan_speed + fanSpeedAdjust(), g_fan_speed_max), g_fan_speed_min);
    } else if (g_temp_current > g_temp_target) {
      fan_speed = fan_speed_off;
    }

    if (fan_speed != g_fan_speed) {
      set_fan_speed(fan_speed);
    } else {
      ESP_LOGD("main", "fan speed unchanged"); 
    }
  } else {
      ESP_LOGD("main", "process_oven_temp(%f) ignored because \"door open\"", temp_current); 
  }
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
  ESP_LOGD("main", DEVICENAME "/stat received -- %s", job);

  if (!g_haveRetainedProperties) {
    g_haveRetainedProperties = true;
    process_properties(x, true);
  }
  
 
}

void reset() {
  set_fan_speed(0.0);
  g_temp_current_count = 0;
  g_saved_fan_speed = fan_speed_invalid;
}