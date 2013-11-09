/*
 * capacity.h
 * Author: Jonatan Schroeder
 *
 * Comment: the teaching staff may, at its own discretion, replace
 * this file with an equivalent file with different values for the
 * limits.
 */

#ifndef _CAPACITY_H_
#define _CAPACITY_H_

#define min(a,b) \
  ({ __typeof__ (a) _a = (a); \
   __typeof__ (b) _b = (b); \
   _a < _b ? _a : _b; })

// Number of network interface cards
#define NUM_NICS 16

// Metric limit. Any number equal to or larger than this number considers the network unreachable.
#define METRIC_UNREACHABLE 1000
#endif
