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
  std::cout << "2D array parameters: " << numCores << std::endl;
#endif 
  switch(cfg.latencyModelType) {
  default: break;
  case Config::SP_2DMESH:
  case Config::SP_2DTORUS:
    switchDimX = (int) sqrt(cfg.switchesPerChip);
    switchDimY = (int) cfg.switchesPerChip / switchDimX;
    chipsDimX = (int) sqrt(numCores/cfg.tilesPerChip);
    chipsDimY = (int) numCores/cfg.tilesPerChip / chipsDimX;
#ifdef DEBUG
    std::cout << "  Switches per chip: " << cfg.switchesPerChip
      << " (" << switchDimX << " x " << switchDimY << ")" << std::endl;
    std::cout << "  System cores:      " << chipsDimX*chipsDimY*cfg.tilesPerChip 
      << " (" << chipsDimX << " x " << chipsDimY << " x " 
      << cfg.tilesPerChip << " tiles)" << std::endl;
#endif
    break;
  }
}

int LatencyModel::threadLatency() {
    return cfg.latencyThread;
}

int LatencyModel::switchLatency(int hopsOnChip, int hopsOffChip, int numTokens, bool inPacket) {
  // Tweak to make latency model work
  if (hopsOffChip == 0) {
    hopsOnChip++;
  }
  //std::cout<<hopsOnChip<<" on chip, "<<hopsOffChip<<" off"<<std::endl;
  int latency = 0;
  latency += cfg.latencyToken * numTokens;
  // Overhead of opening a route through switches
  if (!inPacket) {
    latency += hopsOffChip > 0 ? cfg.latencyOffChipOpen : 0;
    latency += cfg.latencyHopOpen * hopsOnChip;
    latency += cfg.latencyHopOpen * hopsOffChip;
  }
  // Fixed overhead
  latency += hopsOffChip > 0 ? cfg.latencyOffChip : 0;
  latency += cfg.latencyHop * hopsOnChip;
  latency += cfg.latencyHop * hopsOffChip;
  return latency;
}

int LatencyModel::calc2DArray(int s, int t, int numTokens, bool inPacket) {
  if (s == t) {
    return threadLatency();
  }

  // Source coordinates
  int s_chip = s / cfg.tilesPerChip;
  int s_chipX = s_chip % chipsDimX;
  int s_chipY = s_chip / chipsDimX;
  int s_switch = (s / cfg.tilesPerSwitch) % cfg.switchesPerChip;
  int s_switchX = s_switch % switchDimX;
  int s_switchY = s_switch / switchDimX;
  
  // Destination coordinates
  int t_chip = t / cfg.tilesPerChip;
  int t_chipX = t_chip % chipsDimX;
  int t_chipY = t_chip / chipsDimX;
  int t_switch = (t / cfg.tilesPerSwitch) % cfg.switchesPerChip;
  int t_switchX = t_switch % switchDimX;
  int t_switchY = t_switch / switchDimX;

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
      onChipX = s_chipX > t_chipX ? s_switchX : switchDimX-s_switchX-1;
      onChipX += s_chipX > t_chipX ? switchDimX-t_switchX-1 : t_switchX;
    }
    else {
      onChipX = abs(s_switchX - t_switchX);
    }
    // y-dimension
    if (s_chipY != t_chipY) {
      offChipY = abs(s_chipY - t_chipY);
      onChipY = s_chipY > t_chipY ? s_switchY : switchDimY-s_switchY-1;
      onChipY += s_chipY > t_chipY ? switchDimY-t_switchY-1 : t_switchY;
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
          + (chipsDimX-std::max(s_chipX, t_chipX));
      offChipX = std::min(betweenX, aroundX);
      int dirX;
      if (offChipX == betweenX)
        dirX = s_chipX < t_chipX ? 1 : -1;
      else
        dirX = s_chipX < t_chipX ? -1 : 1;
      onChipX = dirX == -1 ? s_switchX : switchDimX-s_switchX-1;
      onChipX += dirX == -1 ? switchDimX-t_switchX-1 : t_switchX;
    }
    else {
      onChipX = abs(s_switchX - t_switchX);
    }
    // y-dimension
    if (s_chipY != t_chipY) {
      int betweenY = abs(s_chipY - t_chipY);
      int aroundY = std::min(s_chipY, t_chipY) 
          + (chipsDimY-std::max(s_chipY, t_chipY));
      offChipY = std::min(betweenY, aroundY);
      int dirY;
      if (offChipY == betweenY)
        dirY = s_chipY < t_chipY ? 1 : -1;
      else
        dirY = s_chipY < t_chipY ? -1 : 1;
      onChipY = dirY == -1 ? s_switchY : switchDimY-s_switchY-1;
      onChipY += dirY == -1 ? switchDimY-t_switchY-1 : t_switchY;
    }
    else {
      onChipY = abs(s_switchY - t_switchY);
    }
    break;
  }

  return switchLatency(onChipX+onChipY, offChipX+offChipY, numTokens, inPacket);
}

int LatencyModel::calcHypercube(int s, int t, int numTokens, bool inPacket) {
  if (s == t) {
    return threadLatency();
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
      return switchLatency(maxHopsOnChip, hopsOffChip, numTokens, inPacket);
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
      return switchLatency(numHops, 0, numTokens, inPacket);
      /*if (!inPacket) {
        return 10;
      } else {
        return 8;
      }*/
    }
  }
}

int LatencyModel::calcClos(int s, int t, int numTokens, bool inPacket) {
  if (s == t) {
    return threadLatency();
  }
  else {
    // If attached to the same edge switch
    if ((int)(s/cfg.tilesPerSwitch) == (int)(t/cfg.tilesPerSwitch)) {
      return switchLatency(0, 0, numTokens, inPacket);
    }
    // Otherwise traverse edge-core-edge switches
    else {
      switch(numCores) {
      default: assert(0);
      case 16:
      case 64:
      case 256: 
        return switchLatency(0, 2, numTokens, inPacket);
      case 1024:
        return switchLatency(2, 2, numTokens, inPacket);
      }
    }
  }
}

ticks_t LatencyModel::calc(uint32_t sCore, uint32_t sNode, 
    uint32_t tCore, uint32_t tNode, int numTokens, bool inPacket) {
 
  uint32_t s = ((sNode>>4) * cfg.tilesPerChip) + sCore;
  uint32_t t = ((tNode>>4) * cfg.tilesPerChip) + tCore;

  // Check the cache first
  /*std::pair<std::pair<uint32_t, uint32_t>, bool> key = std::make_pair(
      std::make_pair(std::min(s, t), std::max(s, t)), inPacket);
  if (cache.count(key) > 0) {
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

