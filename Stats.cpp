// Copyright (c) 2011 Richard Osbourne, 2012 Steve Kerrison, All rights reserved
// This software is freely distributable under a derivative of the
// University of Illinois/NCSA Open Source License posted in
// LICENSE.txt and at <http://github.xcore.com/>

#include "Stats.h"
#include "SystemState.h"
#include "Thread.h"
#include "Node.h"
#include "Core.h"
#include "Resource.h"
#include "Exceptions.h"
#include <iomanip>
#include <sstream>
#include <cstring>


Stats Stats::instance;

void Stats::updateStats(const Thread &t, const char *name) {
  std::string s(name);
  int cid = t.getParent().getCoreID(),
    tid = t.getID().num();
  std::map<std::string, long long*>::iterator iter = istats.find(s);
  if (iter == istats.end())
  {
    long long *x = new long long[cores * NUM_THREADS]();
    istats.insert(std::pair<std::string, long long*>(name,x));
    iter = istats.find(s);
  }
  long long *ts = iter->second;
  ts[(NUM_THREADS * cid) + tid] += 1;
  return;
}

void Stats::dump() {
  //TODO: Get the chip rev. Hard coded for xsim compatibility for now...
  std::cout <<
"InstructionCount:\n"
"-----------------\n";
  long long *counts;
  int threads = cores * NUM_THREADS;
  for (std::map<std::string, long long*>::iterator iter = istats.begin();
    iter != istats.end(); iter++)
  {
    std::cout << "xs1b_" << iter->first << " -";
    counts = iter->second;
    for (int t = 0; t < threads; t += 1)
    {
      std::cout << " " << counts[t];
    }
    std::cout << "\n";
  }
  return;
}

void Stats::initStats(const int cores) {
  Stats::cores = cores;
}
