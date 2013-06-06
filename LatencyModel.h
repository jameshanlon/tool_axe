#ifndef _LatencyModel_h_
#define _LatencyModel_h_

#include <map>
#include "Config.h"

class LatencyModel {
public:
  static LatencyModel instance;
  void init();
  ticks_t calc(uint32_t sCore, uint32_t sNode, 
      uint32_t tCore, uint32_t tNode, int numTokens, bool inPacket);
  static LatencyModel &get() { return instance; }

private:
  LatencyModel() {};
  int numCores;
  std::map<std::pair<std::pair<uint32_t, uint32_t>, bool>, ticks_t> cache;
  
  int threadLatency();
  int switchLatency(int hopsOnChip, int hopsOffChip, int numTokens, bool inPacket);

  // Dimensions for mesh and tori
  int switchDimX; // Number of switches in each dimension of a chip
  int switchDimY; // Number of switches in each dimension of a chip
  int chipsDimX;  // Number of chips in each dimension of the system
  int chipsDimY;  // Number of chips in each dimension of the system
  int calc2DMesh(int s, int t, int numTokens, bool inPacket);
  
  int calcClos(int s, int t, int numTokens, bool inPacket);
  //int calcHypercube(int s, int t, int numTokens, bool inPacket);
  //int calcTree(int s, int t, int numTokens, bool inPacket);
};

#endif // _LatencyModel_h_

