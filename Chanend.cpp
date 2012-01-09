// Copyright (c) 2011, Richard Osborne, All rights reserved
// This software is freely distributable under a derivative of the
// University of Illinois/NCSA Open Source License posted in
// LICENSE.txt and at <http://github.xcore.com/>

#include "Chanend.h"
#include "Core.h"
#include "Node.h"
#include "SystemState.h"
#include "TokenDelay.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cstring>

void Chanend::debug() {
  std::cout << std::setw(6) << (uint64_t) getOwner().time << " ";
  std::cout << "[c" << getOwner().getParent().getCoreID();
  std::cout << "t"<<getOwner().getID() << "] ";
}

bool Chanend::canAcceptToken()
{
  return !buf.full();
}

bool Chanend::canAcceptTokens(unsigned tokens)
{
  return buf.remaining() >= tokens;
}

void Chanend::receiveDataToken(ticks_t time, uint8_t value)
{
  buf.push_back(Token(value));
  update(time);
  //debug(); std::cout << "Got 1 data token" << std::endl;
}

void Chanend::receiveDataTokens(ticks_t time, uint8_t *values, unsigned num)
{
  for (unsigned i = 0; i < num; i++) {
    buf.push_back(Token(values[i]));
  }
  update(time);
  //debug(); std::cout << "Got " << num << " data tokens at " << time << std::endl;
}

void Chanend::receiveCtrlToken(ticks_t time, uint8_t value)
{
  switch (value) {
  case CT_END:
    buf.push_back(Token(value, true));
    release(time);
    break;
  case CT_PAUSE:
    release(time);
    break;
  default:
    buf.push_back(Token(value, true));
    break;
  }
  update(time);
  //debug(); std::cout << "Got a control token " << (int)value << " at " << time << std::endl;
}

void Chanend::notifyDestClaimed(ticks_t time)
{
  if (pausedOut) {
    pausedOut->time = time;
    pausedOut->schedule();
    pausedOut = 0;
  }
}

// TODO can this be merged with the above function.
void Chanend::notifyDestCanAcceptTokens(ticks_t time, unsigned tokens)
{
  if (pausedOut) {
    pausedOut->time = time;
    pausedOut->schedule();
    pausedOut = 0;
  }
}

bool Chanend::openRoute()
{
  if (inPacket)
    return true;
  if (!dest) {
    // TODO if dest in unset should give a link error exception.
    junkPacket = true;
  } else if (!dest->claim(this, junkPacket)) {
    return false;
  }
  inPacket = true;
  return true;
}

bool Chanend::setData(ThreadState &thread, uint32_t value, ticks_t time)
{
  updateOwner(thread);
  if (inPacket)
    return false;
  ResourceID destID(value);
  if (destID.type() != RES_TYPE_CHANEND &&
      destID.type() != RES_TYPE_CONFIG)
    return false;
  dest = thread.getParent().getChanendDest(destID);
  return true;
}

ticks_t Chanend::getLatency(Chanend *dest) {
  unsigned sourceID = getOwner().getParent().getCoreNumber();
  unsigned destID = dest->getOwner().getParent().getCoreNumber();
  return latencyModel->calc(sourceID, destID);
}

Resource::ResOpResult Chanend::
outt(ThreadState &thread, uint8_t value, ticks_t time)
{
  updateOwner(thread);
  if (!openRoute()) {
    pausedOut = &thread;
    return DESCHEDULE;
  }
  if (junkPacket)
    return CONTINUE;
  if (!dest->canAcceptToken()) {
    pausedOut = &thread;
    return DESCHEDULE;
  }
  //dest->receiveDataToken(time, value);
  TokenDelay *td = new DataTokenDelay(dest, value);
  getOwner().getParent().getParent()->getParent()->scheduleOther(*td, 
      time+getLatency((Chanend*) dest));
  //debug(); std::cout << "Sent a data token at " << time << " with delay 100\n";
  return CONTINUE;
}

Resource::ResOpResult Chanend::
out(ThreadState &thread, uint32_t value, ticks_t time)
{
  updateOwner(thread);
  if (!openRoute()) {
    pausedOut = &thread;
    return DESCHEDULE;
  }
  if (junkPacket)
    return CONTINUE;
  if (!dest->canAcceptTokens(4)) {
    pausedOut = &thread;
    return DESCHEDULE;
  }
  // Channels are big endian
  uint8_t tokens[4] = {
    value >> 24,
    value >> 16,
    value >> 8,
    value
  };
  //dest->receiveDataTokens(time, tokens, 4);
  TokenDelay *td = new DataTokensDelay(dest, tokens, 4);
  getOwner().getParent().getParent()->getParent()->scheduleOther(*td, 
      time+getLatency((Chanend *) dest));
  //debug(); std::cout << "Sent 4 data tokens at "<<time<<" with delay "<<DELAY<<"\n";
  return CONTINUE;
}

Resource::ResOpResult Chanend::
outct(ThreadState &thread, uint8_t value, ticks_t time)
{
  updateOwner(thread);
  if (!openRoute()) {
    pausedOut = &thread;
    return DESCHEDULE;
  }
  if (junkPacket) {
    if (value == CT_END || value == CT_PAUSE) {
      inPacket = false;
      junkPacket = false;
    }
    return CONTINUE;
  }
  if (!dest->canAcceptToken()) {
    pausedOut = &thread;
    return DESCHEDULE;
  }  
  //dest->receiveCtrlToken(time, value);
  TokenDelay *td = new CtrlTokenDelay(dest, value);
  getOwner().getParent().getParent()->getParent()->scheduleOther(*td, 
      time+getLatency((Chanend *) dest));
  //debug(); std::cout << "Sent a control token at " << time << " with delay " << DELAY << "\n";
  if (value == CT_END || value == CT_PAUSE) {
    inPacket = false;
  }
  return CONTINUE;
}

bool Chanend::
testct(ThreadState &thread, ticks_t time, bool &isCt)
{
  updateOwner(thread);
  if (buf.empty()) {
    setPausedIn(thread, false);
    return false;
  }
  isCt = buf.front().isControl();
  return true;
}

bool Chanend::
testwct(ThreadState &thread, ticks_t time, unsigned &position)
{
  updateOwner(thread);
  position = 0;
  unsigned numTokens = std::min(buf.size(), 4U);
  for (unsigned i = 0; i < numTokens; i++) {
    if (buf[i].isControl()) {
      position = i + 1;
      return true;
    }
  }
  if (buf.size() < 4) {
    setPausedIn(thread, true);
    return false;
  }
  return true;
}

uint8_t Chanend::poptoken(ticks_t time)
{
  assert(!buf.empty() && "poptoken on empty buf");
  uint8_t value = buf.front().getValue();
  buf.pop_front();
  if (getSource()) {
    getSource()->notifyDestCanAcceptTokens(time, buf.remaining());
  }
  return value;
}

void Chanend::setPausedIn(ThreadState &t, bool wordInput)
{
  pausedIn = &t;
  waitForWord = wordInput;
}

Resource::ResOpResult Chanend::
intoken(ThreadState &thread, ticks_t time, uint32_t &val)
{
  bool isCt;
  if (!testct(thread, time, isCt)) {
    return DESCHEDULE;
  }
  if (isCt)
    return ILLEGAL;
  val = poptoken(time);
  return CONTINUE;
}

Resource::ResOpResult Chanend::
inct(ThreadState &thread, ticks_t time, uint32_t &val)
{
  bool isCt;
  if (!testct(thread, time, isCt)) {
    return DESCHEDULE;
  }
  if (!isCt)
    return ILLEGAL;
  val = poptoken(time);
  return CONTINUE;
}

Resource::ResOpResult Chanend::
chkct(ThreadState &thread, ticks_t time, uint32_t value)
{
  bool isCt;
  if (!testct(thread, time, isCt)) {
    return DESCHEDULE;
  }
  if (!isCt || buf.front().getValue() != value)
    return ILLEGAL;
  (void)poptoken(time);
  return CONTINUE;
}

Resource::ResOpResult Chanend::
in(ThreadState &thread, ticks_t time, uint32_t &value)
{
  unsigned Position;
  if (!testwct(thread, time, Position))
    return DESCHEDULE;
  if (Position != 0)
    return ILLEGAL;
  value = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
  buf.pop_front(4);
  if (getSource()) {
    getSource()->notifyDestCanAcceptTokens(time, buf.remaining());
  }
  return CONTINUE;
}

void Chanend::update(ticks_t time)
{
  assert(!buf.empty());
  if (eventsPermitted()) {
    event(time);
    return;
  }
  if (!pausedIn)
    return;
  if (waitForWord && buf.size() < 4)
    return;
  pausedIn->time = time;
  pausedIn->schedule();
  pausedIn = 0;
}

void Chanend::run(ticks_t time)
{
  assert(0 && "Shouldn't get here");
}

bool Chanend::seeEventEnable(ticks_t time)
{
  if (buf.empty())
    return false;
  event(time);
  return true;
}
