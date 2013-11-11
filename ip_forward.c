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
    (*state)->tree = radix_insert((*state)->tree, netsize, ip, nic);
  } else {
    (*state)->tree = radix_delete((*state)->tree, netsize, ip);
  }
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
  int rc, nic;

  rc = radix_prefix_lookup(state->tree, 32, ip, &nic);
  print_forwarding(packet_id, (rc == FOUND) ? nic : -1);
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
    key = prefix + (tree->key >> prefix_bits);
    if (tree->has_value)
      fn(key, bits, tree->value, out);
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
  uint8_t bits_rmd, bits_match;

  // calculate number
  if (tree && tree->bits) {
    bits_match = num_prefix_match(tree->key, tree->bits, key, bits);
  } 
  // Is at root
  else if (tree) {
    if (NTH_MSB(key, 1) && tree->right && tree->bits) {
      tree->right =  radix_insert(tree->right, bits, key, value);
    } else if (!NTH_MSB(key, 1) && tree->left) {
      tree->left =  radix_insert(tree->left, bits, key, value);
    }
    return tree;
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
  if (!bits_match) {
      radix_node* new_tree = (radix_node*) malloc(sizeof(radix_node));
      new_tree->bits = 0;
      new_tree->key = 0;
      new_tree->has_value = 0;
      new_tree->value = 0;
      if (NTH_MSB(key, 1)) {
        new_tree->right = radix_insert(NULL, bits, key, value);
        new_tree->left = tree;
      } else {
        new_tree->left = radix_insert(NULL, bits, key, value);
        new_tree->right = tree;
      }
      return new_tree;
  }

  // Same key
  if (tree->bits == bits_match && bits == bits_match) {
    tree->value = value;
    tree->has_value = 1;
  } 

  // This node's key is a prefix to the new node's key
  else if (tree->bits == bits_match && bits > bits_match) {
    bits_rmd = bits - bits_match;
    if (NTH_MSB(key, bits_match + 1)) {
      tree->right = radix_insert(tree->right, bits_rmd, key << bits_match, value);
    } else {
      tree->left = radix_insert(tree->left, bits_rmd, key << bits_match, value);
    }
  }

  // new node's key is a prefix to this node's key
  else if (tree->bits > bits_match && bits == bits_match) {
    bits_rmd = tree->bits - bits_match;
    new_tree = (radix_node*) malloc(sizeof(radix_node));
    new_tree->bits = bits_rmd;
    new_tree->key = tree->key << bits_match;
    new_tree->left = tree->left;
    new_tree->right = tree->right;
    new_tree->has_value = tree->has_value;
    new_tree->value = tree->value;

    if (NTH_MSB(tree->key, bits_match + 1)) {
      tree->right = new_tree;
      tree->left = NULL;
    } else {
      tree->right = NULL;
      tree->left = new_tree;
    }
    tree->bits = bits;
    tree->value = value;
    tree->has_value = 1;
    tree->key = key & LEADING_ONES_32(bits_match);
  }

  // The leading bits match is a prefix to both this node and the new node
  else {
    bits_rmd = bits - bits_match;
    new_tree = (radix_node*) malloc(sizeof(radix_node));
    new_tree->bits = tree->bits - bits_match;
    new_tree->key = tree->key << bits_match;
    new_tree->left = tree->left;
    new_tree->right = tree->right;
    new_tree->has_value = tree->has_value;
    new_tree->value = tree->value;

    if (NTH_MSB(key, bits_match + 1)) {
      tree->right = radix_insert(NULL, bits_rmd, key << bits_match, value);
      tree->left = new_tree;

    } else {
      tree->left= radix_insert(NULL, bits_rmd, key << bits_match, value);
      tree->right = new_tree;
    }

    tree->key = key & LEADING_ONES_32(bits_match);
    tree->bits = bits_match;
    tree->has_value = 0;
    tree->value = 0;
  }

  return tree;
}

radix_node* radix_delete(radix_node *tree, uint8_t bits, uint32_t key) {
  uint8_t bits_match, bits_rmd;
  radix_node* new_tree;

  // printf("delete %u/%u\n", key, bits);
  // printf("prefix %u/%u\n", tree->key, tree->bits);
  if (tree) {
    bits_match = num_prefix_match(tree->key, tree->bits, key, bits);
    bits_rmd = bits - bits_match;
  } else {
    return NULL;
  }

  // This node's key is only a prefix, try to delete in subtree
  if (bits && bits_rmd) {

    // try to delete from subtree first
    if (NTH_MSB(key, bits_match + 1)) {
      tree->right = radix_delete(tree->right, bits_rmd,
          key << bits_match);

    } else {
      tree->left = radix_delete(tree->left, bits_rmd,
          key << bits_match);
    }

    // Key is prefix of at least two other nodes' keys
    if (tree->left && tree->right) {
      return tree;
    }

    // Only has 1 child, merge with child if it has no value
    else if (tree->left) {
      if (!tree->has_value) {
        tree->left->key = tree->key + (tree->left->key >> tree->bits);
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
        tree->right->key = tree->key + (tree->right->key >> tree->bits);
        tree->right->bits += tree->bits;
        new_tree = tree->right;
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
  else if (bits && !bits_rmd) {

    // Key is prefix of at least two other keys, cannot delete
    if (tree->left && tree->right) {
      tree->has_value = 0;
      return tree;
    }

    // Only has 1 child, merge nodes and keys
    else if (tree->left) {
      tree->left->key = tree->key + (tree->left->key >> tree->bits);
      tree->left->bits += tree->bits;
      new_tree = tree->left;
      free(tree);
      return new_tree;
    }

    else if (tree->right) {
      tree->right->key = tree->key + (tree->right->key >> tree->bits);
      tree->right->bits += tree->bits;
      new_tree = tree->right;
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

int radix_prefix_lookup(radix_node *tree, uint8_t bits, uint32_t key, int *value) {
  uint8_t bits_match, bits_rmd;
  int rc;

  if (tree) {
    bits_match = num_prefix_match(tree->key, tree->bits, key, bits);
  } else {
    return NOT_FOUND;
  }

  // Found exact match
  if (tree->bits == bits_match && bits == bits_match && tree->has_value) {
    *value = tree->value;
    return FOUND;
  }

  // this node's key is a prefix to the search key 
  else if (bits_match == tree->bits && bits > bits_match) {
    bits_rmd = bits - bits_match;

    if (NTH_MSB(key, bits_match + 1)) {
      rc = radix_prefix_lookup(tree->right, bits_rmd, key << bits_match, value);
    } else {
      rc = radix_prefix_lookup(tree->left, bits_rmd, key << bits_match, value);
    }

    if (rc != FOUND && tree->has_value) {
      *value = tree->value;
      return FOUND;
    } else {
      return rc;
    }
  }

  return NOT_FOUND;
}

// Return the number of leading bits match
uint8_t num_prefix_match(uint32_t key_1, uint8_t bits_1, 
    uint32_t key_2, uint8_t bits_2) {
  uint32_t xnor;
  uint8_t mask_len = 16;
  uint8_t match = 0;
  uint8_t bits_rmd = 32;

  xnor = ~(key_1 ^ key_2);
  // Zero out the fluke matching bits
  xnor = xnor & LEADING_ONES_32(min(bits_1, bits_2));

  while (mask_len > 0 && bits_rmd > 0) {
    if ((xnor & LEADING_ONES_32(mask_len)) == (uint32_t) LEADING_ONES_32(mask_len)) {
      match += mask_len;
      xnor = xnor << mask_len;
      bits_rmd -= mask_len;

    } else {
      mask_len /= 2;
    }
  }
  return match;
}

void traverseTree(radix_node *tree, uint8_t prefix_bits, uint32_t prefix, uint32_t stack) {
  uint32_t ip;

  if (tree) {
    ip = prefix + (tree->key >> prefix_bits);
    traverseTree(tree->right, prefix_bits + tree->bits, ip, stack+1);

    if (tree->has_value)
      printf("%*s%u.%u.%u.%u/%u->%d\n", stack, "", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
        (ip >> 8) & 0xFF, ip & 0xFF, prefix_bits + tree->bits, tree->value);
    else
      printf("%*s%u.%u.%u.%u/%u\n", stack, "", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
        (ip >> 8) & 0xFF, ip & 0xFF, prefix_bits + tree->bits);

    traverseTree(tree->left, prefix_bits + tree->bits, ip, stack+1);
  }
}
