/*
 * ip_route.c
 * Author:
 */

#include <stdio.h>
#include <stdlib.h>

#include "ip_route.h"

static inline void print_advertisement(uint32_t ip, uint8_t netsize, int nic,
                                       unsigned int metric, unsigned int update_id) {
  printf("A %u.%u.%u.%u/%u %u %u\n"
         "T %u.%u.%u.%u/%u %d\n",
         (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, netsize, metric, update_id,
         (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, netsize, nic);
}

/* This function initializes the state of the router.
 */
router_state initialize_router(void) {
  router_state state = malloc(sizeof(struct router_state));
  state->map = NULL;
  return state;
}

void init_vector_table(vector_table *table) {
  int i;

  for (i = 0; i < NUM_NICS; i++) {
    table->dist[i] = METRIC_UNREACHABLE; 
  }
}

// update the forwarding NIC in table and return NIC
int update_forwarding_nic(vector_table *table) {
  int i, nic, min_dist;
 
  nic = -1; 
  min_dist = METRIC_UNREACHABLE;
  for (i = 0; i < NUM_NICS; i++) {
    if (table->dist[i] < min_dist) {
      min_dist = table->dist[i];
      nic = table->forward_nic = i;
    }
  }
  return nic;
}

/* This function is called for every line corresponding to a routing
 * update. The IP is represented as a 32-bit unsigned integer. The
 * netsize parameter corresponds to the size of the prefix
 * corresponding to the network part of the IP address. The metric
 * corresponds to the value informed by the neighboring router, and
 * does not include the cost to reach that router (which is assumed to
 * be always one). If the update triggers an advertisement, this
 * function prints the advertisement in the standard output.
 */
void process_update(router_state *state, uint32_t ip, uint8_t netsize,
		    int nic, unsigned int metric, unsigned int update_id) {
  subnet net;
  vector_table *table;
  map **m;
  int ad, old_fw_nic, new_fw_nic, old_fw_metric, new_fw_metric ;

  m = &((*state)->map);
  net.address = ip;
  net.size = netsize;
  table = map_lookup(*m, net);
  ad = 0;

  if (!table && metric != METRIC_UNREACHABLE) {
    // new subnet, need to advertise
    ad = 1;
    table = (vector_table*) malloc(sizeof(vector_table));
    init_vector_table(table);
    new_fw_nic = table->forward_nic = nic;
    new_fw_metric = table->dist[nic] = metric + 1;
    *m = map_insert(*m, net, table);

  } else if (table) {
    old_fw_nic = table->forward_nic;
    old_fw_metric = table->dist[table->forward_nic];

    table->dist[nic] = min(metric + 1, (unsigned int) METRIC_UNREACHABLE);
    new_fw_nic = update_forwarding_nic(table);
    new_fw_metric = table->dist[new_fw_nic];

    if (new_fw_nic == -1) {
      // unreachable need to be deleted, need to advertise with advertise
      ad = 1;
      *m = map_delete(*m, net);
      table = NULL;
      new_fw_metric = METRIC_UNREACHABLE;
    } else if (old_fw_nic != new_fw_nic) {
      // forwarding NIC was updated to a different NIC, need to advertise
      ad = 1;
    } else if (old_fw_metric != new_fw_metric) {
      // forwarding NIC not changed, but its metric did, need to advertise
      ad = 1;
    }
  } else {
    // originally unreachable, still unreachable. NO ONE CARES
  }

  if (ad) {
    print_advertisement(ip, netsize, new_fw_nic, new_fw_metric, update_id);
  }
  // traverse(*m, 0);
}

void free_map(map* m) {
  if (!m) return;
  free_map(m->left);
  free_map(m->right);
  free(m->table);
  free(m);
}

/* Destroys all memory dynamically allocated through this state (such
 * as the forwarding table) and frees all resources used by the
 * router.
 */
void destroy_router(router_state state) {
  free_map(state->map);
  state->map = NULL;
}

void traverse(map* m, int depth) {
  uint32_t ip;
  if (!m) return;
  traverse(m->right, depth + 1);
  ip = m->net.address;
  printf("%*s%u.%u.%u.%u/%u -> %d\n",
    depth, "", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, m->net.size, 
    m->table->forward_nic);
  traverse(m->right, depth + 1);
}

int subnet_cmp(subnet net_1, subnet net_2) {
  if (net_1.address == net_2.address) {
    return net_1.size - net_2.size;
  }
  return net_1.address - net_2.address;
}

map* map_insert(map *m, subnet net, vector_table *table) {
  int cmp;

  if (m == NULL) {
    m = (map*) malloc(sizeof(map));
    m->net = net;
    m->table = table;
    m->left = m->right = NULL;
    return m;
  }
  cmp = subnet_cmp(net, m->net);
  if (cmp < 0) {
    m->left = map_insert(m->left, net, table);
  } else if (cmp > 0) {
    m->right = map_insert(m->right, net, table);
  } else {
    m->table = table;
  }
  return m;
}

vector_table* map_lookup(map* m, subnet net) {
  int cmp;

  if (m == NULL) {
    return NULL;
  }
  cmp = subnet_cmp(net, m->net);
  if (cmp != 0) {
    return map_lookup((cmp>0) ? (m->right) : (m->left), net);
  } else {
    return m->table;
  }
}

map* map_delete(map *m, subnet net) {
  int cmp;
  map **pred, *tmp;

  if (m == NULL) {
    return NULL;
  }
  cmp = subnet_cmp(net, m->net);
  if (cmp < 0) {
    m->left = map_delete(m->left, net);
  } else if (cmp > 0) {
    m->right = map_delete(m->right, net);
  } else {
    if (!m->left && !m->right) {
      free(m->table);
      free(m);
      return NULL;
    } else if (m->left && m->right) {
      pred = &(m->left);
      while ((*pred)->right) {
        pred = &((*pred)->right);
      }
      free(m->table);
      m->net = (*pred)->net;
      m->table = (*pred)->table;
      free(*pred);
      *pred = NULL;
    } else {
      tmp = (m->left) ? (m->left) : m->right;
      free(m->table);
      free(m);
      return tmp;
    }
  }
  return m;
}
