// Copyright (c) 2011, Richard Osborne, All rights reserved
// This software is freely distributable under a derivative of the
// University of Illinois/NCSA Open Source License posted in
// LICENSE.txt and at <http://github.xcore.com/>

#ifndef _TokenDelay_h_
#define _TokenDelay_h_

#include "Runnable.h"
#include "ChanEndpoint.h"

class TokenDelay : public Runnable {

public:
  // The Channel end to which num tokens must be delivered at wakeUpTime
  ChanEndpoint *dest;

  TokenDelay(ChanEndpoint *dest) : 
    Runnable(TOKEN_DELAY),
    dest(dest)
  {}

  ~TokenDelay() {};

  virtual void run(ticks_t time) = 0;
};

class CtrlTokenDelay : public TokenDelay {
  uint8_t token;

public:
  CtrlTokenDelay(ChanEndpoint *dest, uint8_t token) :
    TokenDelay(dest),
    token(token)
  {}
  
  ~CtrlTokenDelay() {}
  
  virtual void run(ticks_t time);
};

class DataTokenDelay : public TokenDelay {
  uint8_t token;
  
public:
  DataTokenDelay(ChanEndpoint *dest, uint8_t token) :
    TokenDelay(dest),
    token(token)
  {}
  
  ~DataTokenDelay() {}
  
  virtual void run(ticks_t time);
};

class DataTokensDelay : public TokenDelay {
  uint8_t *tokens;
  unsigned num;

public:
  DataTokensDelay(ChanEndpoint *dest, uint8_t tokens_[], int num) :
    TokenDelay(dest),
    num(num)
  { tokens = new uint8_t[num];
    for(int i=0; i<num; i++)
      tokens[i] = tokens_[i];
  }
  
  ~DataTokensDelay() {
    delete[] tokens;
  }
  
  virtual void run(ticks_t time);
};

#endif // _TokenDelay_h
