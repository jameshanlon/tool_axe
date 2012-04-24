#include <iostream>
#include <cassert>
#include <cmath>
#include <algorithm>
#include "LatencyModel.h"

#define MAX_CACHED 100000
//#define DEBUG 

LatencyModel::LatencyModel(const Config &cfg, int numCores) : 
    cfg(cfg), numCores(numCores) {
#ifdef DEBUG
  std::cout << "Latency model parameters" << std::endl;
  std::cout << "Num cores: " << numCores << std::endl;
#endif 
  switch(cfg.latencyModelType) {
  default: break;
  case Config::SP_2DMESH:
  case Config::SP_2DTORUS:
    switchDim = (int) sqrt(cfg.switchesPerChip);
    chipsDim = (numCores/cfg.tilesPerSwitch) / switchDim;
#ifdef DEBUG
    std::cout << "Switches per chip: " << cfg.switchesPerChip
      << " (" << switchDim << " x " << switchDim << ")" << std::endl;
    std::cout << "System cores:      " << chipsDim*chipsDim*cfg.tilesPerChip 
      << " (" << chipsDim << " x " << chipsDim << " x " 
      << cfg.tilesPerChip << ")" << std::endl;
#endif
    break;
  }
}

int LatencyModel::latency(int hopsOnChip, int hopsOffChip, int numTokens, bool inPacket) {
  //std::cout<<hopsOnChip<<" on chip, "<<hopsOffChip<<" off"<<std::endl;
  if (hopsOnChip + hopsOffChip == 0)
    return cfg.latencyThread;
  int latency = 0;
  //latency += cfg.latencyToken * numTokens;
  latency += cfg.latencyToken * 4;
  // Overhead of opening a route through switches
  //if (!inPacket) {
    latency += hopsOffChip > 0 ? cfg.latencyOffChipOpen : 0;
    latency += cfg.latencyHopOpen * hopsOnChip;
    latency += cfg.latencyHopOpen * hopsOffChip;
  //}
  // Fixed overhead
  latency += hopsOffChip > 0 ? cfg.latencyOffChip : 0;
  latency += cfg.latencyHop * hopsOnChip;
  latency += cfg.latencyHop * hopsOffChip;
  return latency;
}

int LatencyModel::calc2DArray(int s, int t, int numTokens, bool inPacket) {
  // Assume:
  //  - Square array of n processors.
  //  - sqrt(n) is divided by integer value sqrt(m).
  //  - It is always preferential to traverse intra-chip rather than inter-chip.
  
  // Source coordinates
  int s_chip = s / cfg.tilesPerChip;
  int s_chipX = s_chip % chipsDim;
  int s_chipY = s_chip / chipsDim;
  int s_switch = (s / cfg.tilesPerSwitch) % cfg.switchesPerChip;
  int s_switchX = s_switch % switchDim;
  int s_switchY = s_switch / switchDim;
  
  // Destination coordinates
  int t_chip = t / cfg.tilesPerChip;
  int t_chipX = t_chip % chipsDim;
  int t_chipY = t_chip / chipsDim;
  int t_switch = (t / cfg.tilesPerSwitch) % cfg.switchesPerChip;
  int t_switchX = t_switch % switchDim;
  int t_switchY = t_switch / switchDim;

/*#ifdef DEBUG
  std::cout << "\ns (" << s << ") : "
    << "chip " << s_chip << " (" << s_chipX << ", " << s_chipY << "), "
    << "switch " << s_switch << " (" << s_switchX << ", " << s_switchY << ")" 
    << std::endl;
  std::cout << "t (" << t << ") : "
    << "chip " << t_chip << " (" << t_chipX << ", " << t_chipY << "), "
    << "switch " << t_switch << " (" << t_switchX << ", " << t_switchY << ")" 
    << std::endl;
#endif*/
  
  int onChipX = 0;
  int onChipY = 0;
  int offChipX = 0;
  int offChipY = 0;

  switch(cfg.latencyModelType) {
  default: assert(0);
  
  case Config::SP_2DMESH:
    // Inter-thread
    if (s == t) {
      break;
    }
    // x-dimension
    if (s_chipX != t_chipX) {
      offChipX = abs(s_chipX - t_chipX);
      onChipX = s_chipX > t_chipX ? s_switchX : switchDim-s_switchX-1;
      onChipX += s_chipX > t_chipX ? switchDim-t_switchX-1 : t_switchX;
    }
    else {
      onChipX = abs(s_switchX - t_switchX);
    }
    // y-dimension
    if (s_chipY != t_chipY) {
      offChipY = abs(s_chipY - t_chipY);
      onChipY = s_chipY > t_chipY ? s_switchY : switchDim-s_switchY-1;
      onChipY += s_chipY > t_chipY ? switchDim-t_switchY-1 : t_switchY;
    }
    else {
      onChipY = abs(s_switchY - t_switchY);
    }
    break;

  case Config::SP_2DTORUS:
    // Inter-thread
    if (s == t) {
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
      onChipY = abs(s_switchY - t_switchY);
    }
    break;
  }

  return latency(onChipX+onChipY, offChipX+offChipY, numTokens, inPacket);
}

int LatencyModel::calcHypercube(int s, int t, int numTokens, bool inPacket) {
  if (s == t) {
    return latency(0, 0, numTokens, inPacket);
  }
  else {
    int switchS = int(s / cfg.tilesPerSwitch);
    int switchT = int(t / cfg.tilesPerSwitch);
    // Count mismatching bits
    int numHops = 0;
    for(size_t i=0; i<8*sizeof(unsigned); i++) {
      if (((switchS ^ switchT) & (1 << i)) > 0)
          numHops++;
    }
    int maxHopsOnChip = (int)(log(cfg.switchesPerChip) / log(2));
    if (maxHopsOnChip < numHops) {
      int hopsOffChip = numHops - maxHopsOnChip;
      return latency(maxHopsOnChip, hopsOffChip, numTokens, inPacket);
      /*if (!inPacket) {
        switch (hopsOffChip) {
          case 1: return 31;
          case 2: return 41;
          case 3: return 50;
          case 4: return 59;
          default: assert(0);
        }
      } else {
        switch (hopsOffChip) {
          case 1: return 19;
          case 2: return 26;
          case 3: return 32;
          case 4: return 38;
          default: assert(0);
        }
      }*/
    }
    else {
      return latency(numHops, 0, numTokens, inPacket);
      /*if (!inPacket) {
        return 10;
      /} else {
        return 8;
      }*/
    }
  }
}

int LatencyModel::calcClos(int s, int t, int numTokens, bool inPacket) {
  if (s == t) {
    return latency(0, 0, numTokens, inPacket);
  }
  else {
    switch(numCores) {
    default: assert(0);
    case 16:
    case 64:
    case 256: 
      return latency(0, 2, numTokens, inPacket);
    case 1024:
      return latency(2, 2, numTokens, inPacket);
    }
  }
}

int LatencyModel::calcTree(int s, int t, int numTokens, bool inPacket) {
  if (s == t) {
    return latency(0, 0, numTokens, inPacket);
  }
  else {
    int degree = cfg.tilesPerSwitch-1;
    int switchS = int(s / cfg.tilesPerSwitch);
    int switchT = int(t / cfg.tilesPerSwitch);
    int numHops = 2 * (((int)(log(abs(switchS-switchT)) / log(degree))) + 1);
    int maxHopsOnChip = 2 * ((int)(log(cfg.switchesPerChip) / log(degree)));
    if (maxHopsOnChip < numHops) {
      int hopsOffChip = numHops - maxHopsOnChip;
      return latency(maxHopsOnChip, hopsOffChip, numTokens, inPacket);
    }
    else {
      return latency(numHops, 0, numTokens, inPacket);
    }
  }
}

ticks_t LatencyModel::calc(uint32_t sCore, uint32_t sNode, 
    uint32_t tCore, uint32_t tNode, int numTokens, bool inPacket) {
 
  uint32_t s = ((sNode>>4) * cfg.tilesPerChip) + sCore;
  uint32_t t = ((tNode>>4) * cfg.tilesPerChip) + tCore;

  // Check the cache first
  // TODO: numTokens to key
  std::pair<std::pair<uint32_t, uint32_t>, bool> key = std::make_pair(
      std::make_pair(std::min(s, t), std::max(s, t)), inPacket);
  /*if (cache.count(key) > 0) {
    return cache[key];
  }*/

  int latency;

  switch(cfg.latencyModelType) {
  default: assert(0);
  
  case Config::NONE:
    latency = 0;
    break;

  case Config::SP_2DMESH:
  case Config::SP_2DTORUS:
    latency = calc2DArray(s, t, numTokens, inPacket);
    break;
  
  case Config::SP_HYPERCUBE:
    latency = calcHypercube(s, t, numTokens, inPacket);
    break;
  
  case Config::SP_CLOS:
    latency = calcClos(s, t, numTokens, inPacket);
    break;

  case Config::SP_FATTREE:
    latency = calcTree(s, t, numTokens, inPacket);
    break;
  }

#ifdef DEBUG
  std::cout << s << " -> " << t << " : " << latency << std::endl;
#endif

  /*cache.insert(std::make_pair(key, latency));
  if (cache.size() > MAX_CACHED) {
    assert(0 && "Latency cache too large.");
  }*/
  return latency * CYCLES_PER_TICK;
}

