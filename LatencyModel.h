#ifndef _LatencyModel_h_
#define _LatencyModel_h_

#include <map>
#include "Config.h"

class LatencyModel {
public:
  enum ModelType {
    SP_MESH,
    SP_TORUS,
    SP_CLOS,
    SP_FATTREE,
    NONE
  };
  LatencyModel(ModelType t, int n);
  ticks_t calc(int s, int t); 

private:
  ModelType type;
  int numCores;
  std::map<std::pair<int, int>, ticks_t> cache;
  // Dimensions for mesh and tori
  int switchDim; // Number of switches in each dimension of a chip
  int chipsDim;  // Number of chips in each dimension of the system
  int calc2DArray(int s, int t);
};

#endif // _LatencyModel_h_
