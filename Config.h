// Copyright (c) 2011, Richard Osborne, All rights reserved
// This software is freely distributable under a derivative of the
// University of Illinois/NCSA Open Source License posted in
// LICENSE.txt and at <http://github.xcore.com/>

#ifndef _Config_h_
#define _Config_h_

#include <stdint.h>
#include <boost/detail/endian.hpp>

/// Number of threads per core.
#define NUM_THREADS 16

/// Number of synchronisers per core.
// TODO Check number.
#define NUM_SYNCS 16

/// Number of locks per core.
#define NUM_LOCKS 4

/// Number of timers per core.
#define NUM_TIMERS 10

/// Number of channel ends per core.
#define NUM_CHANENDS 32

/// Number of clock blocks per core.
#define NUM_CLKBLKS 6

#define NUM_1BIT_PORTS 16
#define NUM_4BIT_PORTS 6
#define NUM_8BIT_PORTS 4
#define NUM_16BIT_PORTS 4
#define NUM_32BIT_PORTS 2
#define NUM_PORTS \
(NUM_1BIT_PORTS + NUM_4BIT_PORTS + NUM_8BIT_PORTS +\
 NUM_16BIT_PORTS + NUM_32BIT_PORTS)

/// Log base 2 of memory size in bytes.
#define RAM_SIZE_LOG 16 // 0.07MB
//#define RAM_SIZE_LOG 20 // 1.05MB
//#define RAM_SIZE_LOG 22 // 4.19MB

/// Default size of ram in bytes
#define RAM_SIZE (1 << RAM_SIZE_LOG)
/// Default ram base
//#define RAM_BASE RAM_SIZE
#define RAM_BASE (1 << 16)

/// Size of the (input) buffer in a chanend.
#define CHANEND_BUFFER_SIZE 8

/// Number of processor cycles per 100MHz timer tick
#define CYCLES_PER_TICK 4

// Assume 10ns cycle (400Mhz clock)
#define CYCLES_PER_SEC (400*1000000)

/// Number of cycles a memory access takes to complete
#define MEMORY_ACCESS_CYCLES 1 

// Latency model parameters (cycles)
#define LATENCY_SWITCH    3  // Latency in and out of the switch
#define LATENCY_THREAD    1  // Between threads
#define LATENCY_ON_CHIP   5  // 1 hop
#define LATENCY_OFF_CHIP  10 // 1 hop

// 2D mesh and torus topology parameters
#define SWITCHES_PER_CHIP 4 // Must be a positive power of 2
#define CORES_PER_SWITCH  4 // Must be a power of 2 greater than 1
#define CORES_PER_CHIP    (SWITCHES_PER_CHIP*CORES_PER_SWITCH)

typedef uint64_t ticks_t;

#define EXPENSIVE_CHECKS 0

#ifdef __GNUC__
#define DIRECT_THREADED
#define UNUSED(x) x __attribute__((__unused__))
#endif // __GNUC__

#ifndef UNUSED
#define UNUSED(x) x
#endif

#if defined(BOOST_LITTLE_ENDIAN)
#define HOST_LITTLE_ENDIAN 1
#elif defined(BOOST_BIG_ENDIAN)
#define HOST_LITTLE_ENDIAN 0
#else
#error "Unknown endianness"
#endif

#endif // _Config_h_
