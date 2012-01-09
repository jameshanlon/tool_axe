#ifndef _LatencyModel_h_
#define _LatencyModel_h_

#include "Config.h"

class LatencyModel {
public:
  enum Type {
    SP_MESH,
    SP_TORUS,
    NONE
  };
  LatencyModel(Type t) : type(t) {}
  ticks_t calc(unsigned int s, unsigned int t); 

private:
  Type type;
};

#endif // _LatencyModel_h_
