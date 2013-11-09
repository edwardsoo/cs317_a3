/*
 *  ip_route.h
 *  Author: Jonatan Schroeder
 */

#ifndef _IP_ROUTE_H_
#define _IP_ROUTE_H_

#include <stdint.h>

#include "capacity.h"

#define min(a,b) \
  ({ __typeof__ (a) _a = (a); \
   __typeof__ (b) _b = (b); \
   _a < _b ? _a : _b; })

typedef struct subnet {
  uint32_t address;
  uint8_t size;
} subnet;

typedef struct vector_table {
  int forward_nic;
  int dist[NUM_NICS];
} vector_table;

typedef struct map {
  subnet net;
  vector_table *table;
  struct map *left, *right;
} map;

typedef struct router_state {
  map* map;
} *router_state;

void traverse(map* m, int depth);
int subnet_cmp(subnet net_1, subnet net_2);
map* map_insert(map *m, subnet net, vector_table *table);
vector_table* map_lookup(map* m, subnet net);
map* map_delete(map *m, subnet net);

router_state initialize_router(void);
void process_update(router_state *state, uint32_t ip, uint8_t netsize,
		    int nic, unsigned int metric, unsigned int update_id);
void destroy_router(router_state state);

#endif
