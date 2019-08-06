#include "esphome.h"

void process_properties(const JsonObject& x, bool fromStat=false);
void set_fan_speed(float fan_speed);
void process_oven_temp(float cur_temp);
void process_stat(const JsonObject& x);
void send_properties();
void on_boot();
void reset();