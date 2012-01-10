#ifndef _LatencyModel_h_
#define _LatencyModel_h_

#include <map>
#include "Config.h"

class LatencyModel {
public:
  enum ModelType {
    SP_MESH,
    SP_TORUS,
    NONE
  };
  LatencyModel(ModelType t, int n);
  ticks_t calc(int s, int t); 

private:
  ModelType type;
  int numCores;
  int numChips;
  int nDim; // sqrt(num cores)
  int mDim; // sqrt(chips per core)
  int oDim; // nDim / oDim
  std::map<std::pair<int, int>, ticks_t> cache;
};

#endif // _LatencyModel_h_
