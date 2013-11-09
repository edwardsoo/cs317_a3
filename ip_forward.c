/*
 * ip_forward.c
 * Author:
 */

#include <stdio.h>
#include <stdlib.h>

#include "ip_forward.h"

/* Helper function that prints the output of a frame being forwarded. */
static inline void print_forwarding(unsigned int packet_id, int nic) {
  printf("O %u %d\n", packet_id, nic);
}

/* Helper function that prints a forwarding table entry. */
static inline void print_forwarding_table_entry(uint32_t ip, uint8_t netsize, int nic, FILE *output) {
  fprintf(output, "%u.%u.%u.%u/%u %d\n",
      (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, netsize, nic);
}

/* This function initializes the state of the router.
 */
router_state initialize_router(void) {
  router_state router = (router_state) malloc(sizeof(struct router_state));
  router->tree = NULL;
  return router;
}

/* This function is called for every line corresponding to a table
 * entry. The IP is represented as a 32-bit unsigned integer. The
 * netsize parameter corresponds to the size of the prefix
 * corresponding to the network part of the IP address. Nothing is
 * printed as a result of this function.
 */
void populate_forwarding_table(router_state *state, uint32_t ip, uint8_t netsize, int nic) {
  // printf("\nINSERTS %u.%u.%u.%u/%u->%d:\n", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, netsize, nic);
  if (nic != -1) {
    (*state)->tree = radix_insert((*state)->tree, netsize, ip >> (32-netsize), nic);
  } else {
    (*state)->tree = radix_delete((*state)->tree, netsize, ip >> (32-netsize));
  }
  // traverseTree((*state)->tree, 0, 0, 0);
}

/* This function is called for every line corresponding to a packet to
 * be forwarded. The IP is represented as a 32-bit unsigned
 * integer. The forwarding table is consulted and the packet is
 * directed to the smallest subnet (longest prefix) in the table that
 * contains the informed destination IP. A line is printed with the
 * information about this forwarding (the function print_forwarding is
 * called in this case).
 */
void forward_packet(router_state state, uint32_t ip, unsigned int packet_id) {
  print_forwarding(packet_id, radix_prefix_lookup(state->tree, 32, ip));
}

/* Prints the current state of the router forwarding table. This
 * function will call the function print_forwarding_table_entry for
 * each valid entry in the forwarding table, in order of prefix
 * address (or in order of netsize if prefix is the same).
 */
void print_router_state(router_state state, FILE *output) {
  radix_inorder_print(state->tree, 0, 0, print_forwarding_table_entry, output);
}

void radix_inorder_print(radix_node *tree, uint8_t prefix_bits, uint32_t prefix,
    void (*fn)(uint32_t, uint8_t, int, FILE*), FILE* out) {
  uint32_t key;
  uint8_t bits;

  if (tree) {
    bits = prefix_bits + tree->bits;
    key = (prefix << tree->bits) + tree->key;
    if (tree->has_value)
      fn(key << (32-bits), bits, tree->value, out);
    radix_inorder_print(tree->left, prefix_bits + tree->bits, key, fn, out);
    radix_inorder_print(tree->right, prefix_bits + tree->bits, key, fn, out);
  }
}

void free_radix(radix_node *tree) {
  if (!tree) return;
  free_radix(tree->left);
  free_radix(tree->right);
  free(tree);
}

/* Destroys all memory dynamically allocated through this state (such
 * as the forwarding table) and frees all resources used by the
 * router.
 */
void destroy_router(router_state state) {
  free_radix(state->tree);
  state->tree = NULL;
}


/******************************************************************************
 *  Radix tree impl
 *****************************************************************************/


radix_node* 
radix_insert(radix_node *tree, uint8_t bits, uint32_t key, int value) {
  radix_node *new_tree;
  uint8_t bits_left, num_match;

  // calculate number
  if (tree) {
    num_match = num_prefix_match(tree->key, tree->bits, key, bits);
  } else {
    new_tree = (radix_node*) malloc(sizeof(radix_node));
    new_tree->bits = bits;
    new_tree->key = key;
    new_tree->has_value = 1;
    new_tree->value = value;
    new_tree->left = new_tree->right = NULL;
    return new_tree;
  }

  // no match
  if (!num_match) {
    radix_node* new_tree = (radix_node*) malloc(sizeof(radix_node));
    new_tree->bits = 0;
    new_tree->key = 0;
    new_tree->has_value = 0;
    new_tree->value = 0;
    if (NTH_LSB(key, bits)) {
      new_tree->right = radix_insert(NULL, bits, key, value);
      new_tree->left = tree;
    } else {
      new_tree->left = radix_insert(NULL, bits, key, value);
      new_tree->right = tree;
    }
    return new_tree;
  }
  // Same key
  if (tree->bits == num_match && bits == num_match) {
    tree->value = value;
    tree->has_value = 1;
  } 
  // This node's key is a prefix to the new node's key
  else if (tree->bits == num_match && bits > num_match) {
    bits_left = bits - num_match;
    if (NTH_LSB(key, bits_left)) {
      tree->right = radix_insert(tree->right, bits_left, 
          key & TRAILING_ONES_32(bits_left), value);
    } else {
      tree->left = radix_insert(tree->left, bits_left,
          key & TRAILING_ONES_32(bits_left), value);
    }
  }
  // new node's key is a prefix to this node's key
  else if (tree->bits > num_match && bits == num_match) {
    bits_left = tree->bits - num_match;
    new_tree = (radix_node*) malloc(sizeof(radix_node));
    new_tree->bits = bits_left;
    new_tree->key = tree->key & TRAILING_ONES_32(bits_left);
    new_tree->left = tree->left;
    new_tree->right = tree->right;
    new_tree->has_value = tree->has_value;
    new_tree->value = tree->value;

    if (NTH_LSB(tree->key, bits_left)) {
      tree->right = new_tree;
      tree->left = NULL;
      // tree->right = radix_insert(tree->right, bits_left, 
      //     tree->key & TRAILING_ONES_32(bits_left), tree->value);
    } else {
      tree->right = NULL;
      tree->left = new_tree;

      // tree->left = radix_insert(tree->left, bits_left,
      //     tree->key & TRAILING_ONES_32(bits_left), tree->value);
    }
    tree->bits = bits;
    tree->value = value;
    tree->has_value = 1;
    tree->key = key;
  }
  // The leading bits match is a prefix to both this node and the new node
  else {
    bits_left = bits - num_match;
    new_tree = (radix_node*) malloc(sizeof(radix_node));
    new_tree->bits = tree->bits - num_match;
    new_tree->key = tree->key & TRAILING_ONES_32(tree->bits - num_match);
    new_tree->left = tree->left;
    new_tree->right = tree->right;
    new_tree->has_value = tree->has_value;
    new_tree->value = tree->value;

    if (NTH_LSB(key, bits_left)) {
      tree->right = radix_insert(NULL, bits_left, 
          key & TRAILING_ONES_32(bits_left), value);
      bits_left = tree->bits - num_match;
      tree->left = new_tree;
    } else {
      tree->left= radix_insert(NULL, bits_left, 
          key & TRAILING_ONES_32(bits_left), value);
      bits_left = tree->bits - num_match;
      tree->right = new_tree;
    }

    tree->key = (key >> (bits - num_match)) & TRAILING_ONES_32(num_match);
    tree->bits = num_match;
    tree->has_value = 0;
    tree->value = 0;
  }

  return tree;
}

radix_node* radix_delete(radix_node *tree, uint8_t bits, uint32_t key) {
  uint8_t num_match, bits_left;
  radix_node* new_tree;

  //printf("%s\n", str);
  //printf("delete %u/%u\n", key, bits);
  //printf("prefix %u/%u\n", tree->key, tree->bits);
  // traverseTree(tree, 0, 0, 0);
  if (tree) {
    num_match = num_prefix_match(tree->key, tree->bits, key, bits);
    bits_left = bits - num_match;
  } else {
    return NULL;
  }

  // This node's key is only a prefix, try to delete in subtree
  if (bits && bits_left) {

    // try to delete from subtree first
    if (NTH_LSB(key, bits_left)) {
      tree->right = radix_delete(tree->right, bits_left,
          key & TRAILING_ONES_32(bits_left));
    } else {
      tree->left = radix_delete(tree->left, bits_left,
          key & TRAILING_ONES_32(bits_left));
    }

    // Key is prefix of at least two other nodes' keys
    if (tree->left && tree->right) {
      return tree;
    }
    // Only has 1 child, merge with child if it has no value
    else if (tree->left) {
      if (!tree->has_value) {
        tree->left->key += (tree->key << tree->left->bits);
        tree->left->bits += tree->bits;
        new_tree = tree->left;
        free(tree);
        return new_tree;
      } else {
        return tree;
      }
    }
    else if (tree->right) {
      if (!tree->has_value) {
        tree->right->key += (tree->key << tree->right->bits);
        tree->right->bits += tree->bits;
        new_tree = tree->left;
        free(tree);
        return new_tree;
      } else {
        return tree;
      }
    }
    // Has no children, delete node if it has no value
    else {
      if (!tree->has_value) {
        free(tree);
        tree = NULL;
      }
      return tree;
    }
  }
  // This node has the key
  else if (bits && !bits_left) {
    // Key is prefix of at least two other nodes' keys, cannot delete
    if (tree->left && tree->right) {
      tree->has_value = 0;
      return tree;
    }
    // Only has 1 child, merge nodes and keys
    else if (tree->left) {
      tree->left->key += (tree->key << tree->left->bits);
      tree->left->bits += tree->bits;
      new_tree = tree->left;
      free(tree);
      return new_tree;
    }
    else if (tree->right) {
      tree->right->key += (tree->key << tree->right->bits);
      tree->right->bits += tree->bits;
      new_tree = tree->left;
      free(tree);
      return new_tree;
    }
    // Has no children, delete node
    else {
      free(tree);
      return NULL;
    }
  }
  return tree;
}

int radix_prefix_lookup(radix_node *tree, uint8_t bits, uint32_t key) {
  uint8_t num_match, bits_left;
  int value;

  if (tree) {
    // printf("key %u bits %u\n", key, bits);
    num_match = num_prefix_match(tree->key, tree->bits, key, bits);
  } else {
    return -1;
  }

  // Found exact match
  if (tree->bits == num_match && bits == num_match && tree->has_value) {
    return tree->value;
  }
  // this node's key is a prefix to the search key 
  else if (num_match == tree->bits && bits > num_match) {
    bits_left = bits - num_match;
    if (NTH_LSB(key, bits_left)) {
      value = radix_prefix_lookup(tree->right, bits_left,
          key & TRAILING_ONES_32(bits_left));
    } else {
      value = radix_prefix_lookup(tree->left, bits_left,
          key & TRAILING_ONES_32(bits_left));
    }
    if (value != -1) {
      return value;
    } else if (tree->has_value) {
      return tree->value;
    }
  }
  return -1;
}

// Return the number of leading bits match
uint8_t num_prefix_match(uint32_t key_1, uint8_t bits_1, 
    uint32_t key_2, uint8_t bits_2) {
  uint32_t s_key_1, s_key_2, xnor;
  uint8_t mask_len = 16;
  uint8_t match = 0;
  uint8_t bits_left = 32;

  s_key_1 = (key_1 << (32-bits_1));
  s_key_2 = (key_2 << (32-bits_2));
  xnor = ~(s_key_1 ^ s_key_2);

  // Zero out the fluke matching bits
  xnor = xnor & LEADING_ONES_32(min(bits_1, bits_2));
  // printf("key1 %u, key2 %u, xnor %u\n", s_key_1, s_key_2, xnor);

  while (mask_len > 0 && bits_left > 0) {
    if ((xnor & LEADING_ONES_32(mask_len)) == LEADING_ONES_32(mask_len)) {
      match += mask_len;
      xnor = xnor << mask_len;
      bits_left -= mask_len;

    } else {
      mask_len /= 2;
    }
  }

  // printf("%u/%u match %u/%u %u bits\n", key_1, bits_1, key_2, bits_2, match);
  return match;
}

void traverseTree(radix_node *tree, uint8_t prefix_bits, uint32_t prefix, uint32_t stack) {
  uint32_t key, ip;

  if (tree) {
    key = (prefix << tree->bits) + tree->key;
    ip = key << (32 - prefix_bits - tree->bits);
    traverseTree(tree->right, prefix_bits + tree->bits, key, stack+1);
    if (tree->has_value)
      printf("%*s%u.%u.%u.%u/%u->%d\n", stack, "", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
        (ip >> 8) & 0xFF, ip & 0xFF, prefix_bits + tree->bits, tree->value);
    else
      printf("%*s%u.%u.%u.%u/%u\n", stack, "", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
        (ip >> 8) & 0xFF, ip & 0xFF, prefix_bits + tree->bits);
    traverseTree(tree->left, prefix_bits + tree->bits, key, stack+1);
  }
}
