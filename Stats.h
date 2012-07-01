// Copyright (c) 2011 Richard Osbourne, 2012 Steve Kerrison, All rights reserved
// This software is freely distributable under a derivative of the
// University of Illinois/NCSA Open Source License posted in
// LICENSE.txt and at <http://github.xcore.com/>

#ifndef _Stats_h_
#define _Stats_h_

#include "TerminalColours.h"
#include "Thread.h"
#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include <map>

class Stats {
private:
  Stats() :
    statsEnabled(false) {}
  public:
  bool statsEnabled;
  static Stats instance;
  int cores;
  std::map<std::string, long long*> istats;
public:
  void setStatsEnabled(bool enable) { statsEnabled = enable; }
  bool getStatsEnabled() const { return statsEnabled; }
  void initStats(const int cores);
  void updateStats(const Thread &t, const char *name);
  void dump();
  static Stats &get()
  {
    return instance;
  }
};

#endif //_Stats_h_
