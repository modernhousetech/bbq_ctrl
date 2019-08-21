#include "esphome.h"

void process_properties(const JsonObject& x, bool fromStat=false);
void set_fan_speed(float fan_speed);
void process_temp_received(float temp, bool external);
void process_oven_temp(float cur_temp);
void process_stat(const JsonObject& x);
float adjust_raw_temp(float temp);
void send_properties();
void on_boot();
void reset();