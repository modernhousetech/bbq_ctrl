#include "esphome.h"
#include "temperature_sensor.h"
using namespace esphome;


#define _countof(arr) sizeof(arr) / sizeof(arr[0])


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
};

class CelsiusTranslationUnit: public TranslationUnit {
  public:
  virtual const char* uom_text() { return "°C"; } 
  virtual float native_to_uom(float native_unit) { return native_unit; }
  virtual float uom_to_native(float uom_unit) { return uom_unit; }
};


// The main device app object definition
class dApp {

    TemperatureSensor* temp_sersors_[4];

    class Probe {
      public:
      float temperature = 0.0;
      float target = 0.0;
      float fan_speed = -1;
      
      JsonObject& toJson() const;
    };
    
    // app_ is either "bbqmini" or "bbqmax". It is passed to us in on_boot()
    std::string app_;

    int lcd_cols_ = 0;
    int lcd_rows_ = 0;

    // We want to remove the variables below as they are now
    // duplicated in probes_
    float oven_temp_target_ = 0.0;
    float oven_temp_current_ = 0.0;

    Probe probes_[4];

    // bbqmini has 1 probe, bbqmax has 4 probes
    int probes_count_ = 0;

    // How many oven_temp_current calls have we received. We really only
    // care about 0 and >0. If > 0 then on a set oven_temp_target we will
    // immediatly set fan speed accordingly.
    int oven_temp_current_count_ = 0;

    float fan_speed_ = 0.0;

    // Fan speed before door open
    // -1 for no saved fan speed
    const float fan_speed_invalid = -1.0;
    float saved_fan_speed_ = fan_speed_invalid;

    // bool enable_probe_ = true;
    bool use_probe_ = true;

    // Fan has never been on. As an option we can set the fan initially to top speed to
    // power it up.
    bool fan_initialized_ = false;

    float fan_speed_min_ = 0.5;
    float fan_speed_max_ = 1.0;
    const float fan_speed_off = 0.0;
    // How much we bump fan speed on every
    // too low oven temp.
    float fan_speed_adjust_ = 0.1;
    // because the internal temp updates much more frequently than
    // we divide the speed adjust by the value below
    // when on internal probe.
    const float fan_speed_internal_factor = 4.0;
    inline float fanSpeedAdjust() { 
        return fan_speed_adjust_ / (use_probe_ ? fan_speed_internal_factor : 1.0 );
    }
    bool door_open_ = false;

    bool haveRetainedProperties_ = false;

    FahrenheitTranslationUnit fahrenheit_xlate_;
    CelsiusTranslationUnit celsius_xlate_;

    // Setup initially for fahrenheit
    TranslationUnit* xlate_ = &fahrenheit_xlate_;

    // We prefix out mqtt messages with this prefix. The value originates in the 
    // yaml layer and is passed to us in on_boot. 
    std::string mqtt_topic_prefix_; 

    // MQTT topics
    std::string mqttTopicStat_ = "/stat";
    std::string mqttTopicProp_ = "/prop";

    DelayAction<> *delayaction_;


public:
    dApp();

    void on_boot(const char* app, int lcd_cols, int lcd_rows, const char* pinProbe0, const char* pinProbe1);
    void process_properties(const JsonObject& jo, bool fromStat=false);
    void set_fan_speed(float fan_speed);
    void process_temp_received(int probeId, float temp, bool external);
    void process_oven_temp(float cur_temp);
    void process_stat(const JsonObject& x);
    float adjust_raw_temp(float temp);
    void send_properties();
    void reset();

    static void on_probe_validity_change(int id, bool is_valid);

protected:
    void makeMqttTopics(const std::string& prefix);
    void set_fan_speed_min(float fan_speed_min);
    void set_door_open(bool door_open);
    JsonObject& make_json(JsonObject& jo, const char* prop_name=nullptr);
    void send_property(const char* prop_name);
    void get_display_values(
      int iProbe, 
      float& temperature,
      float& target,
      float& fan_speed
      );
    void fmt_display_mini(char* line0, char*line1);
    char* fmt_display_line(char* to, int iProbe);
    void refresh_display();
    void set_fan_speed_max(float fan_speed_max);
    void on_probe_validity_change_(int id, bool is_valid);
};

extern dApp dapp;