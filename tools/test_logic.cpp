#include "../firmware/WeatherStation/logic.h"
#include <cassert>
#include <cstdio>
int main(){
 assert(validReading(25,60)); assert(!validReading(NAN,60));
 assert(!validReading(25,101)); assert(!validReading(81,60));
 Alarm a; assert(!a.update(35,80)); assert(a.update(35.1,60));
 assert(a.update(34,60)); assert(!a.update(33,60));
 assert(a.update(25,81)); assert(a.update(25,77)); assert(!a.update(25,75));
 assert(!elapsed(10,0xfffffff0u,30)); assert(elapsed(20,0xfffffff0u,30));
 puts("PASS: valid/invalid sensor data, thresholds, hysteresis, millis rollover");
}
