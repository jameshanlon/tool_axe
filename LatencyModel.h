#ifndef _LatencyModel_h_
#define _LatencyModel_h_

#include <map>
#include "Config.h"

class LatencyModel {
public:
  LatencyModel(const Config &cfg, int numCores);
  ticks_t calc(uint32_t sCore, uint32_t sNode, 
      uint32_t tCore, uint32_t tNode);

private:
  const Config &cfg;
  int numCores;
  std::map<std::pair<int, int>, ticks_t> cache;
  
  int latency(int hopsOnChip, int hopsOffChip);

  // Dimensions for mesh and tori
  int switchDim; // Number of switches in each dimension of a chip
  int chipsDim;  // Number of chips in each dimension of the system
  int calc2DArray(int s, int t);
  
  int calcHypercube(int s, int t);
  int calcClos(int s, int t);
  int calcTree(int s, int t);
};

#endif // _LatencyModel_h_
