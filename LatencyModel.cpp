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

LatencyModel::LatencyModel(ModelType t, int n) : 
  type(t), numCores(n) {
    assert(numCores % CORES_PER_CHIP == 0);
    numChips = numCores / CORES_PER_CHIP;
    nDim = (int) sqrt(numCores);
    mDim = (int) sqrt(CORES_PER_CHIP);
    oDim = nDim / mDim;
    //std::cout << "nDim = " << nDim << std::endl;
    //std::cout << "mDim = " << mDim << std::endl;
    //std::cout << "oDim = " << oDim << std::endl;
}

ticks_t LatencyModel::calc(int s, int t) {
 
  // Check the cache first
  std::pair<int, int> key = std::make_pair(std::min(s, t), std::max(s, t));
  if (cache.count(key) > 0)
    return cache[key];

  // Source coordinates
  int s_chip = s / CORES_PER_CHIP;
  int s_chipX = s_chip % oDim;
  int s_chipY = s_chip / oDim;
  int s_core = s % CORES_PER_CHIP;
  int s_coreX = s_core % mDim;
  int s_coreY = s_core / mDim;
  
  // Destination coordinates
  int t_chip = t / CORES_PER_CHIP;
  int t_chipX = t_chip % oDim;
  int t_chipY = t_chip / oDim;
  int t_core = t % CORES_PER_CHIP;
  int t_coreX = t_core % mDim;
  int t_coreY = t_core / mDim;

  /*std::cout << "\ns (" << s << ") : "
    << "chip " << s_chip << " (" << s_chipX << ", " << s_chipY << "), "
    << "core " << s_core << " (" << s_coreX << ", " << s_coreY << ")" 
    << std::endl;
  std::cout << "t (" << t << ") : "
    << "chip " << t_chip << " (" << t_chipX << ", " << t_chipY << "), "
    << "core " << t_core << " (" << t_coreX << ", " << t_coreY << ")" 
    << std::endl;*/
  
  int onChipX, onChipY, offChipX, offChipY;
  int latency;

  switch(type) {
  default: assert(0);
  
  case NONE:
    latency = 0;
    break;

  case SP_MESH:
    // Inter-thread
    if (s == t) {
      latency = LATENCY_THREAD;
      break;
    }
    // x-dimension
    if (s_chipX != t_chipX) {
      offChipX = abs(s_chipX - t_chipX);
      onChipX = s_chipX > t_chipX ? s_coreX : mDim-s_coreX-1;
      onChipX += s_chipX > t_chipX ? mDim-t_coreX-1 : t_coreX;
    }
    else {
      offChipX = 0;
      onChipX = abs(s_coreX - t_coreX);
    }
    // y-dimension
    if (s_chipY != t_chipY) {
      offChipY = abs(s_chipY - t_chipY);
      onChipY = s_chipY > t_chipY ? s_coreY : mDim-s_coreY-1;
      onChipY += s_chipY > t_chipY ? mDim-t_coreY-1 : t_coreY;
    }
    else {
      offChipY = 0;
      onChipY = abs(s_coreY - t_coreY);
    }

    latency = LATENCY_ON_CHIP * (onChipX + onChipY) +
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
          + (oDim-std::max(s_chipX, t_chipX));
      offChipX = std::min(betweenX, aroundX);
      int dirX;
      if (offChipX == betweenX)
        dirX = s_chipX < t_chipX ? 1 : -1;
      else
        dirX = s_chipX < t_chipX ? -1 : 1;
      onChipX = dirX == -1 ? s_coreX : mDim-s_coreX-1;
      onChipX += dirX == -1 ? mDim-t_coreX-1 : t_coreX;
    }
    else {
      offChipX = 0;
      onChipX = abs(s_coreX - t_coreX);
    }
    // y-dimension
    if (s_chipY != t_chipY) {
      int betweenY = abs(s_chipY - t_chipY);
      int aroundY = std::min(s_chipY, t_chipY) 
          + (oDim-std::max(s_chipY, t_chipY));
      offChipY = std::min(betweenY, aroundY);
      int dirY;
      if (offChipY == betweenY)
        dirY = s_chipY < t_chipY ? 1 : -1;
      else
        dirY = s_chipY < t_chipY ? -1 : 1;
      onChipY = dirY == -1 ? s_coreY : mDim-s_coreY-1;
      onChipY += dirY == -1 ? mDim-t_coreY-1 : t_coreY;
    }
    else {
      offChipY = 0;
      onChipY = abs(s_coreY - t_coreY);
    }
    
    latency = LATENCY_ON_CHIP * (onChipX + onChipY) +
              LATENCY_OFF_CHIP * (offChipX + offChipY);
    break;
  }
  
  //std::cout << s << " -> " << t << " : " << latency << std::endl;
  cache.insert(std::make_pair(key, latency));
  if (cache.size() > MAX_CACHED) {
    assert(0 && "Latency cache too large.");
  }
  return latency;
}

