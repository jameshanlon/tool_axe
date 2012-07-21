#include <fstream>
#include <iostream>
#include <string.h>
#include <iomanip>

#include "Config.h"

#define BUF_LEN 1000 
#define READ_VAL_PARAM(key, var) \
if (!strncmp(key, line, strlen(key))) { \
  sscanf(line, key " %[(:-~)*]%u", str, &(var)); \
  continue; \
}
#define PRINT_PARAM(key, val) \
do { \
  std::cout.width(22); \
  std::cout << std::left << key; \
  std::cout << val << std::endl; \
} while(0)

Config Config::instance;

int Config::read(const std::string &file) {
  FILE *fp = fopen(file.c_str(), "r");
  char line[BUF_LEN];
  char junk[BUF_LEN];
  char str[BUF_LEN];

  if(!fp) {
    std::cout << "No config file.\n";
    return 0;
  }
  
  // Read configuration parameters
  while(fscanf(fp, "%[^\n]\n", line) != EOF) {
    READ_VAL_PARAM("tiles-per-switch",         tilesPerSwitch);
    READ_VAL_PARAM("switches-per-chip",        switchesPerChip);
    READ_VAL_PARAM("latency-global-memory",    latencyGlobalMemory);
    READ_VAL_PARAM("latency-local-memory",     latencyLocalMemory);
    READ_VAL_PARAM("latency-thread",           latencyThread);
    READ_VAL_PARAM("latency-token",            latencyToken);
    READ_VAL_PARAM("latency-tile-switch",      latencyTileSwitch);
    READ_VAL_PARAM("latency-switch",           latencySwitch);
    READ_VAL_PARAM("latency-closed-switch",    latencySwitchClosed);
    READ_VAL_PARAM("latency-serialisation",    latencySerialisation);
    READ_VAL_PARAM("latency-link-on-chip",     latencyLinkOnChip);
    READ_VAL_PARAM("latency-link-off-chip",    latencyLinkOffChip);
    if (!strncmp("latency-model", line, strlen("latency-model"))) {
      sscanf(line, "latency-model%[^\"]\"%[^\"]\"", junk, str);
      if (!strncmp("sp-2dmesh", str, strlen("sp-2dmesh"))) {
        latencyModelType = SP_2DMESH;
      }
      else if (!strncmp("sp-2dtorus", str, strlen("sp-2dtorus"))) {
        latencyModelType = SP_2DTORUS;
      }
      else if (!strncmp("sp-hypercube", str, strlen("sp-hypercube"))) {
        latencyModelType = SP_HYPERCUBE;
      }
      else if (!strncmp("sp-clos", str, strlen("sp-clos"))) {
        latencyModelType = SP_CLOS;
      }
      else if (!strncmp("none", str, strlen("none"))) {
        latencyModelType = NONE;
      }
      else {
        std::cout << "Error: invalid latency model.\n";
        return 0;
      }
      continue;
    }
    std::cerr << "Invalid configuration parameter: " << line << std::endl;
    return 0;
  }
  
  // (Re)calculate consequential parameters
  latencyGlobalMemory *= CYCLES_PER_TICK;
  latencyLocalMemory *= CYCLES_PER_TICK;
  tilesPerChip = switchesPerChip * tilesPerSwitch;

  return 1;
}

void Config::display() {
  // Fixed system parameters
  double ramSizeKB = (double) RAM_SIZE / 1000.0;
  double coreFreqMHz = (double) CYCLES_PER_SEC / 1000000.0;
  std::cout.fill('=');
  std::cout.width(38);
  std::cout << std::left << "System parameters " << std::endl;
  std::cout.fill(' ');
  std::cout << "Num threads per core:         " 
    << NUM_THREADS << std::endl;
  std::cout << "Num synchronisers per core:   " 
    << NUM_SYNCS << std::endl;
  std::cout << "Num locks per core:           " 
    << NUM_LOCKS << std::endl;
  std::cout << "Num timers per core:          " 
    << NUM_TIMERS << std::endl;
  std::cout << "Num channel ends per core:    " 
    << NUM_CHANENDS << std::endl;
  std::cout << "Memory size per core:         " 
    << std::setprecision(4) << ramSizeKB << "KB" << std::endl;
  std::cout << "Core frequency:               " 
    << std::setprecision(4) << coreFreqMHz << "MHz" << std::endl;
  std::cout << "Thread cycles:                "
    << INSTRUCTION_CYCLES << std::endl;

  // Latency model parameters
  if (latencyModelType != NONE) { 
    std::cout.fill('=');
    std::cout.width(40);
    std::cout << std::left << "Latency model parameters " << std::endl;
    std::cout.fill(' ');
    PRINT_PARAM("Switches per chip",      switchesPerChip);
    PRINT_PARAM("Tiles per switch",       tilesPerSwitch);
    PRINT_PARAM("Tiles per chip",         tilesPerChip);
    PRINT_PARAM("Latency global memory",  latencyGlobalMemory/CYCLES_PER_TICK);
    PRINT_PARAM("Latency local memory",   latencyLocalMemory/CYCLES_PER_TICK);
    PRINT_PARAM("Latency thread",         latencyThread);
    PRINT_PARAM("Latency token",          latencyToken);
    PRINT_PARAM("Latency tile to switch", latencyTileSwitch);
    PRINT_PARAM("Latency switch",         latencySwitch);
    PRINT_PARAM("Latency switch closed",  latencySwitchClosed);
    PRINT_PARAM("Latency serialisation",  latencySerialisation);
    PRINT_PARAM("Latency link on-chip",   latencyLinkOnChip);
    PRINT_PARAM("Latency link off-chip",  latencyLinkOffChip);
  }
}

