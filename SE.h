// Copyright (c) 2011, Richard Osborne, All rights reserved
// This software is freely distributable under a derivative of the
// University of Illinois/NCSA Open Source License posted in
// LICENSE.txt and at <http://github.xcore.com/>

#ifndef _SE_h_
#define _SE_h_

#include "XE.h"
#include <stdint.h>

class SE : public XE {
public:
  SE(const char *filename);
  void read();
  int getNumCores() {
    return numCores;
  }
  ~SE();

private:
  uint32_t numCores;
};

#endif //_SE_h_
