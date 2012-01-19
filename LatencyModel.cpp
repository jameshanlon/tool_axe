#include <iostream>
#include <cassert>
#include <cmath>
#include <algorithm>
#include "LatencyModel.h"
#include "Config.h"

/*
 * n := number of processors
 * m := number of processors per chip
 * Assume:
 *  - Square array of n processors.
 *  - sqrt(n) is divided by integer value sqrt(m).
 *  - It is always preferential to traverse intra-chip rather than inter-chip.
 */

#define MAX_CACHED 100000
#define DEBUG 1

LatencyModel::LatencyModel(ModelType t, int n) : 
    type(t), numCores(n) {
  switchDim = (int) sqrt(SWITCHES_PER_CHIP);
  chipsDim = (numCores/CORES_PER_SWITCH) / switchDim;
#ifdef DEBUG
  std::cout << "Num cores:         " << numCores << std::endl;
  std::cout << "Switches per chip: " << SWITCHES_PER_CHIP 
    << " (" << switchDim << " x " << switchDim << ")" << std::endl;
  std::cout << "Cores per switch:  " << CORES_PER_SWITCH << std::endl;
  std::cout << "Cores per chip:    " << CORES_PER_CHIP << std::endl;
  std::cout << "System cores:      " << chipsDim*chipsDim*CORES_PER_CHIP 
    << " (" << chipsDim << " x " << chipsDim << " x " 
    << CORES_PER_CHIP << ")" << std::endl;
#endif
}

int LatencyModel::calc2DArray(int s, int t) {
  
  // Source coordinates
  int s_chip = s / CORES_PER_CHIP;
  int s_chipX = s_chip % chipsDim;
  int s_chipY = s_chip / chipsDim;
  int s_switch = (s / CORES_PER_SWITCH) % SWITCHES_PER_CHIP;
  int s_switchX = s_switch % switchDim;
  int s_switchY = s_switch / switchDim;
  
  // Destination coordinates
  int t_chip = t / CORES_PER_CHIP;
  int t_chipX = t_chip % chipsDim;
  int t_chipY = t_chip / chipsDim;
  int t_switch = (t / CORES_PER_SWITCH) % SWITCHES_PER_CHIP;
  int t_switchX = t_switch % switchDim;
  int t_switchY = t_switch / switchDim;

#ifdef DEBUG
  std::cout << "\ns (" << s << ") : "
    << "chip " << s_chip << " (" << s_chipX << ", " << s_chipY << "), "
    << "switch " << s_switch << " (" << s_switchX << ", " << s_switchY << ")" 
    << std::endl;
  std::cout << "t (" << t << ") : "
    << "chip " << t_chip << " (" << t_chipX << ", " << t_chipY << "), "
    << "switch " << t_switch << " (" << t_switchX << ", " << t_switchY << ")" 
    << std::endl;
#endif
  
  int onChipX, onChipY, offChipX, offChipY;
  int latency;

  switch(type) {
  default: assert(0);
  
  case SP_MESH:
    // Inter-thread
    if (s == t) {
      latency = LATENCY_THREAD;
      break;
    }
    // x-dimension
    if (s_chipX != t_chipX) {
      offChipX = abs(s_chipX - t_chipX);
      onChipX = s_chipX > t_chipX ? s_switchX : switchDim-s_switchX-1;
      onChipX += s_chipX > t_chipX ? switchDim-t_switchX-1 : t_switchX;
    }
    else {
      offChipX = 0;
      onChipX = abs(s_switchX - t_switchX);
    }
    // y-dimension
    if (s_chipY != t_chipY) {
      offChipY = abs(s_chipY - t_chipY);
      onChipY = s_chipY > t_chipY ? s_switchY : switchDim-s_switchY-1;
      onChipY += s_chipY > t_chipY ? switchDim-t_switchY-1 : t_switchY;
    }
    else {
      offChipY = 0;
      onChipY = abs(s_switchY - t_switchY);
    }

    latency = LATENCY_SWITCH + 
              LATENCY_ON_CHIP * (onChipX + onChipY) +
              LATENCY_OFF_CHIP * (offChipX + offChipY);
    break;

  case SP_TORUS:
    // Inter-thread
    if (s == t) {
      latency = LATENCY_THREAD;
      break;
    }
    // x-dimension
    if (s_chipX != t_chipX) {
      int betweenX = abs(s_chipX - t_chipX);
      int aroundX = std::min(s_chipX, t_chipX) 
          + (chipsDim-std::max(s_chipX, t_chipX));
      offChipX = std::min(betweenX, aroundX);
      int dirX;
      if (offChipX == betweenX)
        dirX = s_chipX < t_chipX ? 1 : -1;
      else
        dirX = s_chipX < t_chipX ? -1 : 1;
      onChipX = dirX == -1 ? s_switchX : switchDim-s_switchX-1;
      onChipX += dirX == -1 ? switchDim-t_switchX-1 : t_switchX;
    }
    else {
      offChipX = 0;
      onChipX = abs(s_switchX - t_switchX);
    }
    // y-dimension
    if (s_chipY != t_chipY) {
      int betweenY = abs(s_chipY - t_chipY);
      int aroundY = std::min(s_chipY, t_chipY) 
          + (chipsDim-std::max(s_chipY, t_chipY));
      offChipY = std::min(betweenY, aroundY);
      int dirY;
      if (offChipY == betweenY)
        dirY = s_chipY < t_chipY ? 1 : -1;
      else
        dirY = s_chipY < t_chipY ? -1 : 1;
      onChipY = dirY == -1 ? s_switchY : switchDim-s_switchY-1;
      onChipY += dirY == -1 ? switchDim-t_switchY-1 : t_switchY;
    }
    else {
      offChipY = 0;
      onChipY = abs(s_switchY - t_switchY);
    }
    
    latency = LATENCY_SWITCH +
              LATENCY_ON_CHIP * (onChipX + onChipY) +
              LATENCY_OFF_CHIP * (offChipX + offChipY);
    break;
  }

  return latency;
}

ticks_t LatencyModel::calc(int s, int t) {
 
  // Check the cache first
  std::pair<int, int> key = std::make_pair(std::min(s, t), std::max(s, t));
  if (cache.count(key) > 0)
    return cache[key];

  int latency;

  switch(type) {
  default: assert(0);
  
  case NONE:
    latency = 0;
    break;

  case SP_MESH:
  case SP_TORUS:
    latency = calc2DArray(s, t);
    break;
  
  case SP_CLOS:
    // Roughly
    latency = LATENCY_SWITCH + (2 * LATENCY_OFF_CHIP);
    break;

  case SP_FATTREE:
    // TODO
    latency = 0;
    break;
  }

#ifdef DEBUG
  std::cout << s << " -> " << t << " : " << latency << std::endl;
#endif

  cache.insert(std::make_pair(key, latency));
  if (cache.size() > MAX_CACHED) {
    assert(0 && "Latency cache too large.");
  }
  return latency;
}

