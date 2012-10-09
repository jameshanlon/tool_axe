#include <iostream>
#include <cassert>
#include <cmath>
#include <algorithm>
#include "LatencyModel.h"

#define MAX_CACHED 100000
//#define DEBUG 

LatencyModel LatencyModel::instance;

void LatencyModel::init(int n) {
  numCores = n;
#ifdef DEBUG
  std::cout << "Latency model parameters" << std::endl;
  std::cout << "Num cores: " << numCores << std::endl;
  std::cout << "2D array parameters: " << numCores << std::endl;
#endif 
  switch(Config::get().latencyModelType) {
  default: break;
  case Config::SP_2DMESH:
  case Config::SP_2DTORUS:
    switchDimX = (int) sqrt(Config::get().switchesPerChip);
    switchDimY = (int) Config::get().switchesPerChip / switchDimX;
    chipsDimX = (int) sqrt(numCores/Config::get().tilesPerChip);
    chipsDimY = (int) numCores/Config::get().tilesPerChip / chipsDimX;
#ifdef DEBUG
    std::cout << "  Switches per chip: " << Config::get().switchesPerChip
      << " (" << switchDimX << " x " << switchDimY << ")" << std::endl;
    std::cout << "  System cores:      " << chipsDimX*chipsDimY*Config::get().tilesPerChip 
      << " (" << chipsDimX << " x " << chipsDimY << " x " 
      << Config::get().tilesPerChip << " tiles)" << std::endl;
#endif
    break;
  }
}

int LatencyModel::threadLatency() {
  return Config::get().latencyThread;
}

int LatencyModel::switchLatency(int hopsOnChip, int hopsOffChip, 
    int numTokens, bool inPacket) {
  assert(hopsOffChip==0);
  //std::cout<<hopsOnChip<<" on chip, "<<hopsOffChip<<" off"<<std::endl;
  int latency = 0;
  latency += Config::get().latencyToken * numTokens;
  latency += Config::get().latencyTileSwitch * 2;
  // Fixed overhead
  latency += hopsOnChip*Config::get().latencyLinkOnChip;
  latency += hopsOffChip*Config::get().latencyLinkOffChip;
  latency += (hopsOnChip+hopsOffChip>0) ? Config::get().latencySerialisation : 0;
  // Switch latency only
  if (inPacket) {
    latency += (hopsOnChip+hopsOffChip+1)*Config::get().latencySwitch;
  }
  // Latency due to opening a route through switches and contention
  else {
    latency += (hopsOnChip+hopsOffChip+1) * 
      (int) ceil((float) Config::get().latencySwitch /
        Config::get().switchContentionFactor);
    latency += (hopsOnChip+hopsOffChip+1) *
      (int) ceil((float) Config::get().latencySwitchClosed /
         Config::get().switchContentionFactor);
  }
  return latency;
}

int LatencyModel::calc2DArray(int s, int t, int numTokens, bool inPacket) {
  if (s == t) {
    return threadLatency();
  }

  // Source coordinates
  int s_chip = s / Config::get().tilesPerChip;
  int s_chipX = s_chip % chipsDimX;
  int s_chipY = s_chip / chipsDimX;
  int s_switch = (s / Config::get().tilesPerSwitch) % Config::get().switchesPerChip;
  int s_switchX = s_switch % switchDimX;
  int s_switchY = s_switch / switchDimX;
  
  // Destination coordinates
  int t_chip = t / Config::get().tilesPerChip;
  int t_chipX = t_chip % chipsDimX;
  int t_chipY = t_chip / chipsDimX;
  int t_switch = (t / Config::get().tilesPerSwitch) % Config::get().switchesPerChip;
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

  switch(Config::get().latencyModelType) {
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

  case Config::RAND_2DMESH:
  case Config::RAND_2DTORUS:
    assert(0);
    break;
  }

  return switchLatency(onChipX+onChipY, offChipX+offChipY, numTokens, inPacket);
}

int LatencyModel::calcHypercube(int s, int t, int numTokens, bool inPacket) {
  switch(Config::get().latencyModelType) {
  default: assert(0);
  
  case Config::SP_HYPERCUBE:
    if (s == t) {
      return threadLatency();
    }
    else {
      int switchS = int(s / Config::get().tilesPerSwitch);
      int switchT = int(t / Config::get().tilesPerSwitch);
      // Count mismatching bits
      int numHops = 0;
      for(size_t i=0; i<8*sizeof(unsigned); i++) {
        if (((switchS ^ switchT) & (1 << i)) > 0)
            numHops++;
      }
      int maxHopsOnChip = (int)(log(Config::get().switchesPerChip) / log(2));
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
    break;
  
  case Config::RAND_HYPERCUBE:
    assert(0);
    break;
  }
}

// TODO: fix this for new Clos arrangement
int LatencyModel::calcClos(int s, int t, int numTokens, bool inPacket) {
  switch(Config::get().latencyModelType) {
  default: assert(0);
  
  case Config::SP_CLOS:
    if (s == t) {
      return threadLatency();
    }
    else {
      // If attached to the same edge switch
      if ((int)(s/Config::get().tilesPerSwitch) == (int)(t/Config::get().tilesPerSwitch)) {
        return switchLatency(0, 0, numTokens, inPacket);
      }
      // If in the same chip
      else if ((int)(s/Config::get().tilesPerChip) == (int)(t/Config::get().tilesPerChip)) {
        switch(numCores) {
        default: assert(0);
        case 64:
        case 128:
        case 256:
        case 512:
          return switchLatency(2, 0, numTokens, inPacket);
        case 1024:
        case 2048:
        case 4096:
          if ((int)(s/(Config::get().tilesPerSwitch*Config::get().tilesPerSwitch)) ==
              (int)(t/(Config::get().tilesPerSwitch*Config::get().tilesPerSwitch)))
            return switchLatency(2, 0, numTokens, inPacket);
          return switchLatency(4, 0, numTokens, inPacket);
        }
      }
      // Otherwise traverse edge-core-edge switches
      else {
        // Not modelling these
        assert(0);
        return 0;
      }
    }
  case Config::RAND_CLOS:
    assert(0);
    break;
  }
}

ticks_t LatencyModel::calc(uint32_t sCore, uint32_t sNode, 
    uint32_t tCore, uint32_t tNode, int numTokens, bool inPacket) {
 
  uint32_t s = ((sNode>>4) * Config::get().tilesPerChip) + sCore;
  uint32_t t = ((tNode>>4) * Config::get().tilesPerChip) + tCore;

  // Check the cache first
  /*std::pair<std::pair<uint32_t, uint32_t>, bool> key = std::make_pair(
      std::make_pair(std::min(s, t), std::max(s, t)), inPacket);
  if (cache.count(key) > 0) {
    return cache[key];
  }*/

  int latency;

  switch(Config::get().latencyModelType) {
  default: assert(0);
  
  case Config::NONE:
    latency = 0;
    break;

  case Config::SP_2DMESH:
  case Config::SP_2DTORUS:
  case Config::RAND_2DMESH:
  case Config::RAND_2DTORUS:
    latency = calc2DArray(s, t, numTokens, inPacket);
    break;
  
  case Config::SP_HYPERCUBE:
  case Config::RAND_HYPERCUBE:
    latency = calcHypercube(s, t, numTokens, inPacket);
    break;
  
  case Config::SP_CLOS:
  case Config::RAND_CLOS:
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

