// Copyright (c) 2011-2012, Richard Osborne, All rights reserved
// This software is freely distributable under a derivative of the
// University of Illinois/NCSA Open Source License posted in
// LICENSE.txt and at <http://github.xcore.com/>

#ifndef _RunnableQueue_h_
#define _RunnableQueue_h_

#include "Runnable.h"
#include <cassert>
#include <queue>
#include <iostream>

class RunnableQueue {
private:
  Runnable *head;
  bool contains(Runnable &thread) const
  {
    return thread.prev != 0 || &thread == head;
  }
  /*class Compare {
  public:
    bool operator()(const Runnable *r1, const Runnable *r2) const {
      return r1->wakeUpTime > r2->wakeUpTime;
    }
  };
  std::priority_queue<Runnable*, std::vector<Runnable*>, Compare> queue;*/

public:
  RunnableQueue() : head(0) {}
  
  Runnable &front() const
  {
    return *head;
  }
  
  bool empty() const
  {
    return !head;
  }
  
  void remove(Runnable &thread)
  {
    assert(contains(thread));
    if (&thread == head) {
      head = thread.next;
    } else {
      thread.prev->next = thread.next;
    }
    if (thread.next)
      thread.next->prev = thread.prev;
    thread.prev = 0;
  }
  
  // Insert a thread into the queue.
  void push(Runnable &thread, ticks_t time);
  
  void pop()
  {
    assert(!empty());
    if (head->next) {
      head->next->prev = 0;
    }
    head = head->next;
  }
  
  /*Runnable &front() const {
    return *const_cast<Runnable*>(queue.top());
  }
  
  bool empty() const {
    return queue.empty();
  }
  
  // Insert a thread into the queue
  void push(Runnable &thread, ticks_t time) {
    thread.wakeUpTime = time;
    queue.push(&thread);
  }
  
  void pop() {
    assert(queue.size() > 0);
    queue.pop();
  }*/
};

#endif // _RunnableQueue_h_
