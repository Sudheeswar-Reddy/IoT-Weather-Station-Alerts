#pragma once
#include <math.h>
#include <stdint.h>
struct Alarm {
  bool hot=false, humid=false;
  bool update(float t, float h) {
    if (!isfinite(t)||!isfinite(h)||t < -40||t > 80||h < 0||h > 100) return false;
    if (t > 35) hot=true; else if(t <= 33) hot=false;
    if (h > 80) humid=true; else if(h <= 75) humid=false;
    return hot || humid;
  }
};
inline bool validReading(float t,float h) {
 return isfinite(t)&&isfinite(h)&&t>=-40&&t<=80&&h>=0&&h<=100;
}
inline bool elapsed(uint32_t now,uint32_t then,uint32_t interval) {
 return uint32_t(now-then)>=interval;
}
