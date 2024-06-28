#ifndef ROUTING_H
#define ROUTING_H

#include <stdint.h>
#include "array.h"
#include "heap.h"
#include <pthread.h>
#include "array.h"
#include "list.h"
#include "network.h"

#define N_THREADS 32
#define FINALTIMELOCK 40

extern pthread_mutex_t data_mutex;
extern pthread_mutex_t jobs_mutex;
extern struct array** paths;
extern struct element* jobs;

struct thread_args{
  struct network* network;
  struct array* payments;
  uint64_t current_time;
  long data_index;
  enum routing_method routing_method;
};

struct distance{
  long node;
  uint64_t distance;
  uint64_t amt_to_receive;
  uint64_t fee;
  double probability;
  uint32_t timelock;
  double weight;
  long next_edge;
};

struct dijkstra_hop {
  long node;
  long edge;
};

struct path_hop{
  long sender;
  long receiver;
  long edge;
};

struct route_hop {
  long from_node_id;
  long to_node_id;
  long edge_id;
  uint64_t amount_to_forward;
  uint32_t timelock;
  uint64_t edges_lock_start_time;
  uint64_t edges_lock_end_time;
  uint64_t group_cap;
};


struct route {
  uint64_t total_amount;
  uint64_t total_fee;
  uint64_t total_timelock;
  struct array *route_hops;
};

enum pathfind_error{
  NOLOCALBALANCE,
  NOPATH
};

void initialize_dijkstra(long n_nodes, long n_edges, struct array* payments);

uint64_t estimate_capacity(struct edge* edge, struct network* network, enum routing_method routing_method);

void run_dijkstra_threads(struct network* network, struct array* payments, uint64_t current_time, enum routing_method routing_method);

struct array* dijkstra(long source, long destination, uint64_t amount, struct network* network, uint64_t current_time, long p, enum pathfind_error *error, enum routing_method routing_method, struct element* exclude_edges, int without_update_distances);

struct route* transform_path_into_route(struct array* path_hops, uint64_t amount_to_send, struct network* network, uint64_t time);

void print_hop(struct route_hop* hop);

int compare_distance(struct distance* a, struct distance* b);

void free_route(struct route* route);


#endif
