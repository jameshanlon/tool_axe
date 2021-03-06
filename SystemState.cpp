// Copyright (c) 2011, Richard Osborne, All rights reserved
// This software is freely distributable under a derivative of the
// University of Illinois/NCSA Open Source License posted in
// LICENSE.txt and at <http://github.xcore.com/>

#include <iomanip>
#include "SystemState.h"
#include "Node.h"
#include "Core.h"
#include "Trace.h"
#include "Stats.h"
#include "TokenDelay.h"

SystemState::~SystemState()
{
  for (std::vector<Node*>::iterator it = nodes.begin(), e = nodes.end();
       it != e; ++it) {
    delete *it;
  }
}

void SystemState::addNode(std::auto_ptr<Node> n)
{
  n->setParent(this);
  nodes.push_back(n.get());
  n.release();
}

void SystemState::
completeEvent(Thread &t, EventableResource &res, bool interrupt)
{
  if (interrupt) {
    t.regs[SSR] = t.sr.to_ulong();
    t.regs[SPC] = t.getParent().targetPc(t.pc);
    t.regs[SED] = t.regs[ED];
    t.ieble() = false;
    t.inint() = true;
    t.ink() = true;
  } else {
    t.inenb() = 0;
  }
  t.eeble() = false;
  // EventableResource::completeEvent sets the ED and PC.
  res.completeEvent();
  if (Tracer::get().getTracingEnabled()) {
    if (interrupt) {
      Tracer::get().interrupt(t, res, t.getParent().targetPc(t.pc),
                                      t.regs[SSR], t.regs[SPC], t.regs[SED],
                                      t.regs[ED]);
    } else {
      Tracer::get().event(t, res, t.getParent().targetPc(t.pc), t.regs[ED]);
    }
  }
}

ChanEndpoint *SystemState::getChanendDest(ResourceID ID)
{
  unsigned coreID = ID.node();
  // TODO build lookup map.
  
  for (node_iterator outerIt = node_begin(), outerE = node_end();
       outerIt != outerE; ++outerIt) {
    Node &node = **outerIt;
    for (Node::core_iterator innerIt = node.core_begin(),
         innerE = node.core_end(); innerIt != innerE; ++innerIt) {
      Core &core = **innerIt;
      if (core.getCoreID() == coreID) {
        ChanEndpoint *result;
        bool isLocal = core.getLocalChanendDest(ID, result);
        assert(isLocal);
        (void)isLocal;
        return result;
      }
    }
  }
  return 0;
}

int SystemState::run()
{
  try {
    while (!scheduler.empty()) {
      Runnable &runnable = scheduler.front();
      currentRunnable = &runnable;
      scheduler.pop();
      runnable.run(runnable.wakeUpTime);
    }
  } catch (ExitException &ee) {
    return ee.getStatus();
  }
  Tracer::get().noRunnableThreads(*this);
  return 1;
}

void SystemState::threadStats() {
  long totalCount = 0;
  ticks_t maxTime = 0;
  int numCores = 0;
  for (node_iterator nIt=node_begin(), nEnd=node_end(); nIt!=nEnd; ++nIt) {
    Node &node = **nIt;
    std::cout << "Node " << node.getNodeID()
      << " =========================================" << std::endl;
    for (Node::core_iterator cIt=node.core_begin(), cEnd=node.core_end(); 
        cIt!=cEnd; ++cIt) {
      Core &core = **cIt;
      numCores++;
      std::cout << "Core " << core.getCoreNumber() 
        << " -----------------------------------------" << std::endl;
      std::cout 
        << std::setw(8) << "Thread" << " "
        << std::setw(12) << "Time" << " "
        << std::setw(12) << "Insts" << " "
        << std::setw(12) << "Insts/cycle" << std::endl;
      for (int i=0; i<NUM_THREADS; i++) {
        Thread &thread = core.getThread(i);
        totalCount += thread.count;
        maxTime = maxTime > thread.time ? maxTime : thread.time;
        double ratio = (double) thread.count / (double) thread.time;
        std::cout 
          << std::setw(8) << i << " " 
          << std::setw(12) << thread.time << " "
          << std::setw(12) << thread.count << " " 
          << std::setw(12) << std::setprecision(2) << ratio << std::endl;
      }
    }
  }
}

void SystemState::systemStats() {
  long totalCount = 0;
  ticks_t maxTime = 0;
  ticks_t maxCore0Time = 0;
  int numCores = 0;
  for (node_iterator nIt=node_begin(), nEnd=node_end(); nIt!=nEnd; ++nIt) {
    Node &node = **nIt;
    for (Node::core_iterator cIt=node.core_begin(), cEnd=node.core_end(); 
        cIt!=cEnd; ++cIt) {
      Core &core = **cIt;
      numCores++;
      for (int i=0; i<NUM_THREADS; i++) {
        Thread &t = core.getThread(i);
        totalCount += t.count;
        maxTime = maxTime > t.time ? maxTime : t.time;
      }
      if (core.getCoreNumber() == 0) {
        for (int i=0; i<NUM_THREADS; i++) {
          Thread &t = core.getThread(i);
          maxCore0Time = maxCore0Time > t.time ? maxCore0Time : t.time;
        }
      }
    }
  }
  
  // Simulated performance
  double seconds = (double) maxTime / 100000000.0;
  double opsPerSec = (double) totalCount / seconds;
  double gOpsPerSec = opsPerSec / 1000000000.0;
  long peakOpsPerSec = CYCLES_PER_SEC;
  long peakGOpsPerSec = peakOpsPerSec / 1000000000.0;
  double perCentPeak = (100.0/(double) peakOpsPerSec) * opsPerSec;
  double ramSizeKB = (double) RAM_SIZE / 1000.0;
  double aggregateRam = ((double) numCores * ramSizeKB) / 1000.0;
  std::cout << "Simulated performance =========================="
    << std::endl;
  std::cout << "Num cores:                    "
    << numCores << std::endl;
  std::cout << "Aggregate memory:             " 
    << std::setprecision(4) << aggregateRam << "MB" << std::endl;
  std::cout << "Total instructions executed:  "
    << totalCount << std::endl;
  std::cout << "Max thread cycles:            "
    << maxTime << std::endl;
  std::cout << "Max core 0 thread cycles:     "
    << maxCore0Time << std::endl;
  std::cout << "Elapsed time:                 " 
    << std::setprecision(3) << seconds << "s" << std::endl;
  std::cout << "Instructions per second:      "
    << std::setprecision(3) << opsPerSec
    << " (" << std::setprecision(2) << gOpsPerSec << " GIPS)" << std::endl;
  std::cout << "Of peak:                      "
    << std::setprecision(2) << perCentPeak << "\% (" 
    << peakGOpsPerSec << " GIPS)" << std::endl;
  
  // Simulation performance
  /*double opsPerRealSec = (double) totalCount / elapsedTime;
  double gOpsPerRealSec = opsPerRealSec / 1000000000.0;
  double slowdown = opsPerSec / opsPerRealSec;
  std::cout << std::endl;
  std::cout << "Simulation performance =========================" 
    << std::endl;
  std::cout << "Elapsed time:                 "
    << std::setprecision(3) << elapsedTime << "s" << std::endl;
  std::cout << "Instructions per second:      "
    << std::setprecision(3) << opsPerRealSec
    << " (" << std::setprecision(2) << gOpsPerRealSec << " GIPS)" << std::endl;
  std::cout << "Slowdown:                     "
    << std::setprecision(2) << slowdown << "x" << std::endl;*/
}

