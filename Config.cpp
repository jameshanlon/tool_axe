#include <fstream>
#include <iostream>

#include "Config.h"

#define BUF_LEN 1000 
#define READ_PARAM(key, var) \
do { \
  if (!strncmp(key, line, strlen(key))) { \
    sscanf(line, key " %[(:-~)*]%u", str, &(var)); \
    continue; \
  } \
} while(0)
#define PRINT_PARAM(key, val) \
do { \
  std::cout.width(22); \
  std::cout << std::left << key; \
  std::cout << val << std::endl; \
} while(0)

void Config::read(const std::string &file) {
  FILE *fp = fopen(file.c_str(), "r");
  char line[BUF_LEN];
  char str[BUF_LEN];

  if(!fp) {
    std::cout << "No config file.\n";
    std::exit(1);
  }

  // Read configuration parameters
  while(fscanf(fp, "%[^\n]\n", line) != EOF) {
    READ_PARAM("ram-size-log",      ramSizeLog);
    READ_PARAM("switches-per-chip", switchesPerChip);
    READ_PARAM("cores-per-switch",  coresPerSwitch);
    READ_PARAM("latency-memory",    latencyMemory);
    READ_PARAM("latency-switch",    latencySwitch);
    READ_PARAM("latency-thread",    latencyThread);
    READ_PARAM("latency-on-chip",   latencyOnChip);
    READ_PARAM("latency-off-chip",  latencyOffChip);
  }
  
  // Calculate consequential parameters
  ramSize = 1 << ramSizeLog;
  ramBase = 1 << DEFAULT_RAM_SIZE_LOG;
  coresPerChip = switchesPerChip * coresPerSwitch;
}

void Config::display() {
  std::cout.fill('=');
  std::cout.width(26);
  std::cout << std::left << "Configuration " << std::endl;
  std::cout.fill(' ');
  PRINT_PARAM("RAM size (KB)",     ramSize);
  PRINT_PARAM("Switches per chip", switchesPerChip);
  PRINT_PARAM("Cores per switch",  coresPerSwitch);
  PRINT_PARAM("Latency memory",    latencyMemory);
  PRINT_PARAM("Latency switch",    latencySwitch);
  PRINT_PARAM("Latency thread",    latencyThread);
  PRINT_PARAM("Latency on-chip",   latencyOnChip);
  PRINT_PARAM("Latency off-chip",  latencyOffChip);
}

