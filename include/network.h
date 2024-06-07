#ifndef NETWORK_H
#define NETWORK_H

#include <stdio.h>
#include <stdint.h>
#include "cloth.h"
#include "list.h"

#define MAXMSATOSHI 5E17 //5 millions  bitcoin
#define MAXTIMELOCK 100
#define MINTIMELOCK 10
#define MAXFEEBASE 5000
#define MINFEEBASE 1000
#define MAXFEEPROP 10
#define MINFEEPROP 1
#define MAXLATENCY 100
#define MINLATENCY 10
#define MINBALANCE 1E2
#define MAXBALANCE 1E11

enum group_update_type {
    CONSTRUCT,
    CLOSE,
    UPDATE,
};

/* a policy that must be respected when forwarding a payment through an edge (see edge below) */
struct policy {
  uint64_t fee_base;
  uint64_t fee_proportional;
  uint64_t min_htlc;
  uint32_t timelock;
};

/* a node of the payment-channel network */
struct node {
  long id;
  struct array* open_edges;
  unsigned int explored;
};

/* a bidirectional payment channel of the payment-channel network open between two nodes */
struct channel {
  long id;
  long node1;
  long node2;
  long edge1;
  long edge2;
  uint64_t capacity;
  unsigned int is_closed;
//  unsigned int occupied;
//  struct element* payment_history;
};

/* an edge represents one of the two direction of a payment channel */
struct edge {
  long id;
  long channel_id;
  long from_node_id;
  long to_node_id;
  long counter_edge_id;
  struct policy policy;
  uint64_t balance;
  unsigned int is_closed;
  uint64_t tot_flows;
  struct group* group;
  struct element* channel_updates; // results for record
  struct element* edge_locked_balance_and_durations;
};


struct edge_locked_balance_and_duration{
    uint64_t locked_balance;
    uint64_t locked_start_time;
    uint64_t locked_end_time;
};


struct edge_snapshot {
  long id;
  uint64_t balance;
  short is_included_in_group;
  uint64_t group_cap;
  short does_channel_update_exist;
  uint64_t last_success_amount;
  uint64_t last_fail_amount;
  uint64_t sent_amt;
};


struct group_update {
    long group_id;
    long triggered_node_id;
    uint64_t time;
    uint64_t group_cap;
    enum group_update_type type;
    uint64_t* edge_balances;
};


struct group {
    long id;
    struct array* edges;
    uint64_t max_cap_limit;
    uint64_t min_cap_limit;
    uint64_t max_cap;
    uint64_t min_cap;
    uint64_t group_cap;
    uint64_t is_closed; // if not zero, it describes closed time
    uint64_t constructed_time;
    struct element* group_updates;
};


struct graph_channel {
  long node1_id;
  long node2_id;
};


struct network {
  struct array* nodes;
  struct array* channels;
  struct array* edges;
  struct array* groups;
  gsl_ran_discrete_t* faulty_node_prob; //the probability that a nodes in the network has a fault and goes offline
};


struct node* new_node(long id);

struct channel* new_channel(long id, long direction1, long direction2, long node1, long node2, uint64_t capacity);

struct edge* new_edge(long id, long channel_id, long counter_edge_id, long from_node_id, long to_node_id, uint64_t balance, struct policy policy, uint64_t channel_capacity);

void open_channel(struct network* network, gsl_rng* random_generator);

struct network* initialize_network(struct network_params net_params, gsl_rng* random_generator);

struct element* update_group(struct group* group, struct network_params net_params, uint64_t current_time, struct element* group_add_queue, long triggered_node_id, enum group_update_type type, struct network* network);

struct element* construct_groups_from_queue(struct element* group_add_queue, struct network *network, struct network_params net_params, uint64_t current_time);

long get_edge_balance(struct edge* e);

struct edge_snapshot* take_edge_snapshot(struct edge* e, uint64_t sent_amt, short is_included_in_group, uint64_t group_cap);

struct group* new_group(struct edge* requesting_edge, struct network_params net_params, struct network* network, uint64_t current_time);

#endif
