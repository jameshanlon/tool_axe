// Copyright (c) 2011-12, Richard Osborne, All rights reserved
// This software is freely distributable under a derivative of the
// University of Illinois/NCSA Open Source License posted in
// LICENSE.txt and at <http://github.xcore.com/>

#include "Thread.h"
#include "Core.h"
#include "Node.h"
#include "SystemState.h"
#include "Trace.h"
#include "Exceptions.h"
#include "BitManip.h"
#include "SyscallHandler.h"
#include "JIT.h"
#include "CRC.h"
#include "InstructionHelpers.h"
#include "Synchroniser.h"
#include "Chanend.h"
#include "InstructionProperties.h"
#include <iostream>

const char *registerNames[] = {
  "r0",
  "r1",
  "r2",
  "r3",
  "r4",
  "r5",
  "r6",
  "r7",
  "r8",
  "r9",
  "r10",
  "r11",
  "cp",
  "dp",
  "sp",
  "lr",
  "et",
  "ed",
  "kep",
  "ksp",
  "spc",
  "sed",
  "ssr"
};

void Thread::finalize()
{
  scheduler = &getParent().getParent()->getParent()->getScheduler();
}

void Thread::dump() const
{
  std::cout << std::hex;
  for (unsigned i = 0; i < NUM_REGISTERS; i++) {
    std::cout << getRegisterName(i) << ": 0x" << regs[i] << "\n";
  }
  std::cout << "pc: 0x" << getParent().targetPc(pc) << "\n";
  std::cout << std::dec;
}

void Thread::schedule()
{
  getParent().getParent()->getParent()->schedule(*this);
}

bool Thread::setSRSlowPath(sr_t enabled)
{
  if (enabled[EEBLE]) {
    EventableResourceList &EER = eventEnabledResources;
    for (EventableResourceList::iterator it = EER.begin(),
         end = EER.end(); it != end; ++it) {
      if ((*it)->seeOwnerEventEnable(time)) {
        return true;
      }
    }
  }
  if (enabled[IEBLE]) {
    EventableResourceList &IER = interruptEnabledResources;
    for (EventableResourceList::iterator it = IER.begin(),
         end = IER.end(); it != end; ++it) {
      if ((*it)->seeOwnerEventEnable(time)) {
        return true;
      }
    }
  }
  return false;
}

bool Thread::isExecuting() const
{
  return this == parent->getParent()->getParent()->getExecutingRunnable();
}

enum {
  SETC_INUSE_OFF = 0x0,
  SETC_INUSE_ON = 0x8,
  SETC_COND_FULL = 0x1,
  SETC_COND_AFTER = 0x9,
  SETC_COND_EQ = 0x11,
  SETC_COND_NEQ = 0x19,
  SETC_IE_MODE_EVENT = 0x2,
  SETC_IE_MODE_INTERRUPT = 0xa,
  SETC_RUN_STOPR = 0x7,
  SETC_RUN_STARTR = 0xf,
  SETC_RUN_CLRBUF = 0x17,
  SETC_MS_MASTER = 0x1007,
  SETC_MS_SLAVE = 0x100f,
  SETC_BUF_NOBUFFERS = 0x2007,
  SETC_BUF_BUFFERS = 0x200f,
  SETC_RDY_NOREADY = 0x3007,
  SETC_RDY_STROBED = 0x300f,
  SETC_RDY_HANDSHAKE = 0x3017,
  SETC_PORT_DATAPORT = 0x5007,
  SETC_PORT_CLOCKPORT = 0x500f,
  SETC_PORT_READYPORT = 0x5017
};

static void internalError(const Thread &thread, const char *file, int line) {
  std::cout << "Internal error in " << file << ":" << line << "\n"
  << "Register state:\n";
  thread.dump();
  std::abort();
}

static inline uint32_t maskSize(uint32_t x)
{
  assert((x & (x + 1)) == 0 && "Not a valid mkmsk mask");
  return 32 - countLeadingZeros(x);
}

static Resource::Condition setCCondition(uint32_t Val)
{
  switch (Val) {
  default: assert(0 && "Unexpected setc value");
  case SETC_COND_FULL:
    return Resource::COND_FULL;
  case SETC_COND_AFTER:
    return Resource::COND_AFTER;
  case SETC_COND_EQ:
    return Resource::COND_EQ;
  case SETC_COND_NEQ:
    return Resource::COND_NEQ;
  }
}

static Port::ReadyMode
getPortReadyMode(uint32_t val)
{
  switch (val) {
  default: assert(0 && "Unexpected ready mode");
  case SETC_RDY_NOREADY: return Port::NOREADY;
  case SETC_RDY_STROBED: return Port::STROBED;
  case SETC_RDY_HANDSHAKE: return Port::HANDSHAKE;
  }
}

static Port::MasterSlave
getMasterSlave(uint32_t val)
{
  switch (val) {
  default: assert(0 && "Unexpected master slave value");
  case SETC_MS_MASTER: return Port::MASTER;
  case SETC_MS_SLAVE: return Port::SLAVE;
  }
}

static Port::PortType
getPortType(uint32_t val)
{
  switch (val) {
  default: assert(0 && "Unexpected port type");
  case SETC_PORT_DATAPORT: return Port::DATAPORT;
  case SETC_PORT_CLOCKPORT: return Port::CLOCKPORT;
  case SETC_PORT_READYPORT: return Port::READYPORT;
  }
}

bool Thread::
setC(ticks_t time, ResourceID resID, uint32_t val)
{
  Resource *res = getParent().getResourceByID(resID);
  if (!res) {
    return false;
  }
  if (val == SETC_INUSE_OFF || val == SETC_INUSE_ON)
    return res->setCInUse(*this, val == SETC_INUSE_ON, time);
  if (!res->isInUse())
    return false;
  switch (val) {
  default:
    internalError(*this, __FILE__, __LINE__); // TODO
  case SETC_IE_MODE_EVENT:
  case SETC_IE_MODE_INTERRUPT:
    {
      if (!res->isEventable())
        return false;
      EventableResource *ER = static_cast<EventableResource *>(res);
      ER->setInterruptMode(*this, val == SETC_IE_MODE_INTERRUPT);
      break;
    }
  case SETC_COND_FULL:
  case SETC_COND_AFTER:
  case SETC_COND_EQ:
  case SETC_COND_NEQ:
    {
      if (!res->setCondition(*this, setCCondition(val), time))
        return false;
      break;
    }
  case SETC_RUN_STARTR:
  case SETC_RUN_STOPR:
    {
      if (res->getType() != RES_TYPE_CLKBLK)
        return false;
      ClockBlock *clock = static_cast<ClockBlock*>(res);
      if (val == SETC_RUN_STARTR)
        clock->start(*this, time);
      else
        clock->stop(*this, time);
      break;
    }
  case SETC_MS_MASTER:
  case SETC_MS_SLAVE:
    if (res->getType() != RES_TYPE_PORT)
      return false;
    return static_cast<Port*>(res)->setMasterSlave(*this, getMasterSlave(val),
                                                   time);
  case SETC_BUF_BUFFERS:
  case SETC_BUF_NOBUFFERS:
    if (res->getType() != RES_TYPE_PORT)
      return false;
    return static_cast<Port*>(res)->setBuffered(*this, val == SETC_BUF_BUFFERS,
                                                time);
  case SETC_RDY_NOREADY:
  case SETC_RDY_STROBED:
  case SETC_RDY_HANDSHAKE:
    if (res->getType() != RES_TYPE_PORT)
      return false;
    return static_cast<Port*>(res)->setReadyMode(*this, getPortReadyMode(val),
                                                 time);
  case SETC_PORT_DATAPORT:
  case SETC_PORT_CLOCKPORT:
  case SETC_PORT_READYPORT:
    if (res->getType() != RES_TYPE_PORT)
      return false;
    return static_cast<Port*>(res)->setPortType(*this, getPortType(val), time);
  case SETC_RUN_CLRBUF:
    {
      if (res->getType() != RES_TYPE_PORT)
        return false;
      static_cast<Port*>(res)->clearBuf(*this, time);
      break;
    }
  }
  return true;
}

#include "InstructionMacrosCommon.h"

#ifdef DIRECT_THREADED
#define INST(s) s ## _label
#define ENDINST goto *(opcode[PC] + (char*)&&INST(INITIALIZE))
#define OPCODE(s) ((char*)&&INST(s) - (char*)&&INST(INITIALIZE))
#define START_DISPATCH_LOOP ENDINST;
#define END_DISPATCH_LOOP
#else
#define INST(inst) case (inst)
#define ENDINST break
#define OPCODE(s) s
#define START_DISPATCH_LOOP while(1) { switch (opcode[PC]) {
#define END_DISPATCH_LOOP } }
#endif

#define SAVE_CACHED() \
do { \
  this->pc = PC;\
} while(0)
#define REG(Num) this->regs[Num]
#define IMM(Num) (Num)
#define THREAD (*this)
#define CORE (*core)
#define PC this->pc
#define OP(n) (operands[PC].ops[(n)])
#define LOP(n) (operands[PC].lops[(n)])
#define EXCEPTION(et, ed) \
do { \
  PC = exception(THREAD, PC, et, ed); \
  YIELD(PC); \
} while(0);
#define ERROR() \
do { \
  SAVE_CACHED(); \
  internalError(*this, __FILE__, __LINE__); \
} while(0)
#define TRACE(...) \
do { \
  if (tracing) { \
    SAVE_CACHED(); \
    Tracer::get().trace(*this, __VA_ARGS__); \
  } \
} while(0)
#define TRACE_REG_WRITE(register, value) \
do { \
  if (tracing) { \
    Tracer::get().regWrite(register, value); \
  } \
} while(0)
#define TRACE_END() \
do { \
  if (tracing) { \
    Tracer::get().traceEnd(); \
  } \
} while(0)
#define DESCHEDULE(pc) \
do { \
  SAVE_CACHED(); \
  this->waiting() = true; \
  return; \
} while(0)
#define PAUSE_ON(pc, resource) \
do { \
  SAVE_CACHED(); \
  this->waiting() = true; \
  this->pausedOn = resource; \
  return; \
} while(0)
#define YIELD(pc) \
do { \
  if (THREAD.hasTimeSliceExpired()) { \
    SAVE_CACHED(); \
    sys.schedule(*this); \
    return; \
  } \
} while(0)
#define TAKE_EVENT(pc) \
do { \
  SAVE_CACHED(); \
  sys.takeEvent(*this); \
  sys.schedule(*this); \
  return; \
} while(0)
#define SETSR(bits, pc) \
do { \
  SAVE_CACHED(); \
  if (this->setSR(bits)) { \
    sys.takeEvent(*this); \
  } \
  sys.schedule(*this); \
  return; \
} while(0)

#define INSTRUCTION_CYCLES 4

void Thread::run(ticks_t time)
{
  if (Tracer::get().getTracingEnabled())
    runAux<true>(time);
  else
    runAux<false>(time);
}

template <bool tracing>
void Thread::runAux(ticks_t time) {
  SystemState &sys = *getParent().getParent()->getParent();
  Core *core = &this->getParent();
  OPCODE_TYPE *opcode = core->opcode;
  Operands *operands = core->operands;

  // The main dispatch loop. On backward branches and indirect jumps we call
  // YIELD() to ensure one thread which never pauses cannot starve the
  // other threads.
  START_DISPATCH_LOOP
  INST(INITIALIZE):
    getParent().initCache(OPCODE(DECODE), OPCODE(ILLEGAL_PC),
                          OPCODE(ILLEGAL_PC_THREAD), OPCODE(YIELD),
                          OPCODE(SYSCALL), OPCODE(EXCEPTION),
                          OPCODE(JIT_INSTRUCTION));
    ENDINST;
#define EMIT_INSTRUCTION_DISPATCH
#include "InstructionGenOutput.inc"
#undef EMIT_INSTRUCTION_DISPATCH
  // Two operand short.
  INST(TSETMR_2r):
    {
      TRACE("tsetmr ", Register(OP(0)), ", ", SrcRegister(OP(1)));
      TIME += INSTRUCTION_CYCLES;
      Synchroniser *sync = this->getSync();
      if (sync) {
        sync->master().reg(IMM(OP(0))) = REG(OP(1));
      } else {
        // Undocumented behaviour, possibly incorrect. However this seems to
        // match the simulator.
        REG(OP(0)) = REG(OP(1));
      }
      PC++;
      TRACE_END();
    }
    ENDINST;
      
  // Zero operand short
  INST(SSYNC_0r):
    {
      TRACE("ssync");
      TIME += INSTRUCTION_CYCLES;
      Synchroniser *sync = this->getSync();
      // May schedule / deschedule threads
      if (!sync) {
        // TODO is this right?
        DESCHEDULE(PC);
      } else {
        switch (sync->ssync(*this)) {
        case Synchroniser::SYNC_CONTINUE:
          PC++;
          break;
        case Synchroniser::SYNC_DESCHEDULE:
          TRACE_END();
          PAUSE_ON(PC, sync);
          break;
        case Synchroniser::SYNC_KILL:
          {
            free();
            TRACE_END();
            DESCHEDULE(PC);
          }
        }
      }
      TRACE_END();
    }
    ENDINST;
  INST(FREET_0r):
    {
      TRACE("freet");
      if (this->getSync()) {
        // TODO check expected behaviour
        ERROR();
      }
      free();
      TRACE_END();
      DESCHEDULE(PC);
    }
    ENDINST;
  
  // Pseudo instructions.
  INST(SYSCALL):
    int retval;
    this->pc = PC;
    switch (SyscallHandler::doSyscall(*this, retval)) {
    case SyscallHandler::EXIT:
      throw (ExitException(retval));
    case SyscallHandler::DESCHEDULE:
      DESCHEDULE(PC);
      break;
    case SyscallHandler::CONTINUE:
      uint32_t target = TO_PC(REG(LR));
      if (CHECK_PC(target)) {
        PC = target;
        YIELD(PC);
      } else {
        EXCEPTION(ET_ILLEGAL_PC, REG(LR));
      }
      break;
    }
    ENDINST;
  INST(EXCEPTION):
    this->pc = PC;
    SyscallHandler::doException(*this);
    throw (ExitException(1));
    ENDINST;
  INST(ILLEGAL_PC):
    EXCEPTION(ET_ILLEGAL_PC, FROM_PC(PC));
    ENDINST;
  INST(ILLEGAL_PC_THREAD):
    EXCEPTION(ET_ILLEGAL_PC, this->pendingPc);
    ENDINST;
  INST(YIELD):
    PC = THREAD.pendingPc;
    YIELD(PC);
    ENDINST;
  INST(ILLEGAL_INSTRUCTION):
    EXCEPTION(ET_ILLEGAL_INSTRUCTION, 0);
    ENDINST;
  INST(DECODE):
    {
      InstructionOpcode opc;
      instructionDecode(CORE, PC, opc, operands[PC]);
      instructionTransform(opc, operands[PC], CORE, PC);
      if (CORE.invalidationInfo[PC] == Core::INVALIDATE_NONE) {
        CORE.invalidationInfo[PC] = Core::INVALIDATE_CURRENT;
      }
      if (instructionProperties[opc].size == 4) {
        CORE.invalidationInfo[PC + 1] = Core::INVALIDATE_CURRENT_AND_PREVIOUS;
      } else {
        assert(instructionProperties[opc].size == 2);
      }
#ifdef DIRECT_THREADED
      static OPCODE_TYPE opcodeMap[] = {
#define EMIT_INSTRUCTION_LIST
#define DO_INSTRUCTION(inst) OPCODE(inst),
#include "InstructionGenOutput.inc"
#undef EMIT_INSTRUCTION_LIST
#undef DO_INSTRUCTION
      };
      opcode[PC] = opcodeMap[opc];
#else
      opcode[PC] = opc;
#endif
      // Reexecute current instruction.
      ENDINST;
    }
  INST(JIT_INSTRUCTION):
    operands[PC].func(THREAD);
    ENDINST;
  END_DISPATCH_LOOP
}
