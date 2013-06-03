// Copyright (c) 2011, Richard Osborne, All rights reserved
// This software is freely distributable under a derivative of the
// University of Illinois/NCSA Open Source License posted in
// LICENSE.txt and at <http://github.xcore.com/>

#include "Chanend.h"
#include "Core.h"
#include "Node.h"
#include "SystemState.h"
#include "TokenDelay.h"
#include "LatencyModel.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cstring>

#define BYTES_TO_WORD(buf) \
  ((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3])
//#define DEBUG

void Chanend::debug() {
  std::cout << std::setw(6) << (uint64_t) getOwner().time << " ";
  std::cout << "[c" << getOwner().getParent().getCoreID();
  std::cout << "t"<<getOwner().getID() << "] ";
}

void Chanend::illegalMemAccessPacket() {
  std::cout << "Illegal memory access packet." << std::endl;
}

void Chanend::illegalMemAddress() {
  std::cout << "Illegal memory address." << std::endl;
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
  if (memAccessPacket) {
    illegalMemAccessPacket();
    return;
  }
  buf.push_back(Token(value));
  update(time);
#ifdef DEBUG
  debug(); std::cout << "Got 1 data token" << std::endl;
#endif
}

void Chanend::receiveDataTokens(ticks_t time, uint8_t *values, unsigned num)
{
  if (!memAccessPacket) {
    for (unsigned i = 0; i < num; i++) {
      buf.push_back(Token(values[i]));
    }
    update(time);
  }
  else {
    if (num != 4) {
      illegalMemAccessPacket();
      return;
    }
    switch(memAccessType) {
    case READ4:
      switch(memAccessStep) {
      case 0:
        setData(getOwner(), BYTES_TO_WORD(values), time);
        //std::cout<<"Got CRI "<<std::hex<<BYTES_TO_WORD(values)<<std::endl;
        break;
      case 1:
        memAddress = BYTES_TO_WORD(values);
        //std::cout<<"Got address "<<std::hex<<memAddress<<std::endl;
        break;
      default:
        illegalMemAccessPacket();
        return;
      }
      break;
    case WRITE4:
      switch(memAccessStep) {
      case 0:
        setData(getOwner(), BYTES_TO_WORD(values), time);
        //std::cout<<"Got CRI "<<std::hex<<BYTES_TO_WORD(values)<<std::endl;
        break;
      case 1:
        memAddress = BYTES_TO_WORD(values);
        //std::cout<<"Got address "<<std::hex<<memAddress<<std::endl;
        break;
      case 2:
        memValue = BYTES_TO_WORD(values);
        //std::cout<<"Got value "<<memValue<<std::endl;
        break;
      default:
        illegalMemAccessPacket();
        return;
      }
      break;
    default:
      assert(0 && "Invalid memory access type.");
    }
    memAccessStep++;
  }
#ifdef DEBUG
  debug(); std::cout << "Got " << num
    << " data tokens at " << time << std::endl;
#endif
}

void Chanend::receiveCtrlToken(ticks_t time, uint8_t value)
{
  if (!memAccessPacket) {
    switch (value) {
    case CT_END:
      buf.push_back(Token(value, true));
      release(time);
      update(time);
      break;
    case CT_PAUSE:
      release(time);
      update(time);
      break;
    case CT_READ4:
      memAccessPacket = true;
      memAccessType = READ4;
      memAccessStep = 0;
      //std::cout<<"Memory access: READ4"<<std::endl;
      break;
    case CT_WRITE4:
      memAccessPacket = true;
      memAccessType = WRITE4;
      memAccessStep = 0;
      //std::cout<<"Memory access: WRITE4"<<std::endl;
      break;
    default:
      buf.push_back(Token(value, true));
      update(time);
      break;
    }
  }
  else {
    Core &core = getOwner().getParent();
    switch (value) {
    case CT_END:
      // Respond with (value, END) for READ or (END) for WRITE
      // NOTE: this is not being performed by the thread so will not effect
      // that thread.
      switch(memAccessType) {
      case READ4:
        if(memAccessStep != 2) {
          illegalMemAccessPacket();
          return;
        }
        if (!core.isValidAddress(core.physicalAddress(memAddress))) {
          illegalMemAddress();
          return;
        }
        out(getOwner(), core.loadWord(core.physicalAddress(memAddress)),
            time+Config::get().latencyGlobalMemory);
        outct(getOwner(), CT_END, 
            time+Config::get().latencyGlobalMemory+CYCLES_PER_TICK);
        // Update the time of this thread to account for these operations
        getOwner().time += Config::get().latencyGlobalMemory+(2*CYCLES_PER_TICK);
        //debug(); std::cout<<"Reading from address "
        //  <<std::hex<<memAddress<<std::dec<<" = "<<v<<std::endl;
        //std::cout<<"End READ4"<<std::endl;
        break;
      case WRITE4:
        if(memAccessStep != 3) {
          illegalMemAccessPacket();
          return;
        }
        if (!core.isValidAddress(core.physicalAddress(memAddress))) {
          illegalMemAddress();
          return;
        }
        core.storeWord(memValue, core.physicalAddress(memAddress));
        outct(getOwner(), CT_END, 
            time+Config::get().latencyGlobalMemory+CYCLES_PER_TICK);
        // Update the time of this thread to account for these operations
        getOwner().time += Config::get().latencyGlobalMemory+(2*CYCLES_PER_TICK);
        //debug(); std::cout<<"Writing to address "
        //  <<std::hex<<memAddress<<std::dec<<" = "<<memValue<std::endl;
        //std::cout<<"End WRITE4"<<std::endl;
        break;
      }
      memAccessPacket = false;
      release(time);
      break;
    default:
      illegalMemAccessPacket();
      return;
    }
  }
#ifdef DEBUG
  debug(); std::cout << "Got a control token "
      << (int)value << " at " << time << std::endl;
#endif
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

bool Chanend::setData(Thread &thread, uint32_t value, ticks_t time)
{
  updateOwner(thread);
  if (inPacket)
    return false;
  ResourceID destID(value);
  if (destID.type() != RES_TYPE_CHANEND &&
      destID.type() != RES_TYPE_CONFIG)
    return false;
  dest = thread.getParent().getChanendDest(destID);
  if (!dest) 
    std::cout << "Could not SETD" << std::endl;
  return true;
}

ticks_t Chanend::getLatency(Chanend *dest, int numTokens, bool inPacket,
    ticks_t time) {
  // Might be a switch
  if (dest->hasOwner()) {
    uint32_t sourceCore = getOwner().getParent().getCoreNumber();
    uint32_t sourceNode = getOwner().getParent().getParent()->getNodeID();
    uint32_t destCore = dest->getOwner().getParent().getCoreNumber();
    uint32_t destNode = dest->getOwner().getParent().getParent()->getNodeID();
    //std::cout<<sourceNode<<" - "<<destNode<<std::endl;
    ticks_t latency = LatencyModel::get().calc(sourceCore, sourceNode, destCore, destNode,
        numTokens, inPacket);
    // Don't let tokens over take one another
    if (time+latency < lastTime+lastLatency) {
      latency = lastLatency + (time-lastTime);
    }
    lastTime = time;
    lastLatency = latency;
    return latency;
  }
  // Should really return the latency to the switch based on the node it
  // belongs to, but this isn't straight forward as a Chanend in a switch
  // doesn't have an owner.
  return 0;
}

Resource::ResOpResult Chanend::
outt(Thread &thread, uint8_t value, ticks_t time)
{ 
  ticks_t l = getLatency((Chanend *) dest, 1, inPacket, time);
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
  getOwner().getParent().getParent()->getParent()->scheduleOther(*td, time+l);
#ifdef DEBUG
  debug(); std::cout << "Sent a data token at "
      << time << " with delay " << l << std::endl;
#endif
  return CONTINUE;
}

Resource::ResOpResult Chanend::
out(Thread &thread, uint32_t value, ticks_t time)
{
  ticks_t l = getLatency((Chanend *) dest, 4, inPacket, time);
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
    (uint8_t) (value >> 24),
    (uint8_t) (value >> 16),
    (uint8_t) (value >> 8),
    (uint8_t) (value)
  };
  //dest->receiveDataTokens(time, tokens, 4);
  TokenDelay *td = new DataTokensDelay(dest, tokens, 4);
  getOwner().getParent().getParent()->getParent()->scheduleOther(*td, time+l);
#ifdef DEBUG
  debug(); std::cout << "Sent 4 data tokens at "
      << time << " with delay " << l << std::endl;
#endif
  return CONTINUE;
}

Resource::ResOpResult Chanend::
outct(Thread &thread, uint8_t value, ticks_t time)
{
  ticks_t l = getLatency((Chanend *) dest, 1, inPacket, time);
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
  getOwner().getParent().getParent()->getParent()->scheduleOther(*td, time+l);
#ifdef DEBUG
  debug(); std::cout << "Sent a control token at "
      << time << " with delay " << l << std::endl;
#endif
  if (value == CT_END || value == CT_PAUSE) {
    inPacket = false;
  }
  return CONTINUE;
}

bool Chanend::
testct(Thread &thread, ticks_t time, bool &isCt)
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
testwct(Thread &thread, ticks_t time, unsigned &position)
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

void Chanend::setPausedIn(Thread &t, bool wordInput)
{
  pausedIn = &t;
  waitForWord = wordInput;
}

Resource::ResOpResult Chanend::
intoken(Thread &thread, ticks_t time, uint32_t &val)
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
inct(Thread &thread, ticks_t time, uint32_t &val)
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
chkct(Thread &thread, ticks_t time, uint32_t value)
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
in(Thread &thread, ticks_t time, uint32_t &value)
{
  unsigned Position;
  if (!testwct(thread, time, Position))
    return DESCHEDULE;
  if (Position != 0) {
    std::cout << "Illegal: control token in buffer on IN"<<std::endl;
    return ILLEGAL;
  }
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
