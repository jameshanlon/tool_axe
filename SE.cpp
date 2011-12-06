// Copyright (c) 2011, Richard Osborne, All rights reserved
// This software is freely distributable under a derivative of the
// University of Illinois/NCSA Open Source License posted in
// LICENSE.txt and at <http://github.xcore.com/>

#include "SE.h"
#include <cstring>
#include <iostream>

SE::SE(const char *filename) : XE(filename) {
}

void SE::read() {
  char magic[4];
  
  // SIRE header
  s.read(magic, 4);
  if (std::memcmp(magic, "SIRE", 4) != 0) {
    error = true;
    return;
  }
  numCores = ReadU32();

  // XMOS header
  s.read(magic, 4);
  if (std::memcmp(magic, "XMOS", 4) != 0) {
    error = true;
    return;
  }
  version = ReadU16();
  
  // Skip padding
  s.seekg(2, std::ios_base::cur);
  
  // Read sector headers
  while (!ReadHeader()) {}
}

SE::~SE() {
}

