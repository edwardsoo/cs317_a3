/*
 *  ip_forward.h
 *  Author: Jonatan Schroeder
 */

#ifndef _IP_FORWARD_H_
#define _IP_FORWARD_H_

#include <stdint.h>

#include "capacity.h"

#define min(a,b) \
  ({ __typeof__ (a) _a = (a); \
   __typeof__ (b) _b = (b); \
   _a < _b ? _a : _b; })
#define NTH_LSB(X,N) ((X>>(N-1))&0x00000001)
#define NTH_MSB(X,N) ((X<<(N-1))&0x80000000)
#define TRAILING_ONES_32(X) (0xFFFFFFFF >> (32-X))
#define LEADING_ONES_32(X) (0xFFFFFFFF << (32-X))

#define FOUND 1
#define NOT_FOUND 0

typedef struct radix_node {
  uint32_t key;
  uint8_t bits, has_value;
  int value;
  struct radix_node *left, *right;
} radix_node;
radix_node* radix_insert(radix_node *tree, uint8_t bits, uint32_t key, int value); 
radix_node* radix_delete(radix_node *tree, uint8_t bits, uint32_t key);
int radix_prefix_lookup(radix_node *tree, uint8_t bits, uint32_t key, int *value);
uint8_t num_prefix_match(uint32_t key_1, uint8_t bits_1, uint32_t key_2, uint8_t bits_2);
void traverseTree(radix_node *tree, uint8_t prefix_bits, uint32_t prefix, uint32_t stack);
void radix_inorder_print(radix_node *tree, uint8_t prefix_bits, uint32_t prefix,
    void (*fn)(uint32_t, uint8_t, int, FILE*), FILE* out);

typedef struct router_state {
  radix_node *tree;
} *router_state;

router_state initialize_router(void);
void populate_forwarding_table(router_state *state, uint32_t ip, uint8_t netsize, int nic);
void forward_packet(router_state state, uint32_t ip, unsigned int packet_id);
void print_router_state(router_state state, FILE *output);
void destroy_router(router_state state);

#endif
