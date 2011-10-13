// Copyright (c) 2011, Richard Osborne, All rights reserved
// This software is freely distributable under a derivative of the
// University of Illinois/NCSA Open Source License posted in
// LICENSE.txt and at <http://github.xcore.com/>

#ifndef _PeripheralRegistry_h
#define _PeripheralRegistry_h

#include <string>
#include <memory>
#include <map>
#include "AccessSecondIterator.h"

class PeripheralDescriptor;

namespace PeripheralRegistry {
  typedef AccessSecondIterator<std::map<std::string,PeripheralDescriptor*>::iterator> iterator;
  void add(std::auto_ptr<PeripheralDescriptor> p);
  PeripheralDescriptor *get(const std::string &s);
  iterator begin();
  iterator end();
};

#endif // _PeripheralRegistry_h
