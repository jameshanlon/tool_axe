// Copyright (c) 2011, Richard Osborne, All rights reserved
// This software is freely distributable under a derivative of the
// University of Illinois/NCSA Open Source License posted in
// LICENSE.txt and at <http://github.xcore.com/>

#include "TokenDelay.h"

void CtrlTokenDelay::run(ticks_t time) {
  dest->receiveCtrlToken(time, token);
}

void DataTokenDelay::run(ticks_t time) {
  dest->receiveDataToken(time, token);
}

void DataTokensDelay::run(ticks_t time) {
  dest->receiveDataTokens(time, tokens, num);
}
