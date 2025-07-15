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

/* a policy that must be respected when forwarding a payment through an edge (see edge below) */
struct policy {
  uint64_t fee_base;
  uint64_t fee_proportional;
  uint64_t min_htlc;
  uint32_t timelock;
  double cul_threshold;
};

/* a node of the payment-channel network */
struct node {
  long id;
  struct array* open_edges;
  struct element **results;
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
  struct element* channel_updates;
};


struct edge_snapshot {
  long id;
  uint64_t balance;
  short is_in_group;
  uint64_t group_cap;
  short does_channel_update_exist;
  uint64_t last_channle_update_value;
  uint64_t sent_amt;
};


struct channel_update {
    long edge_id;
    uint64_t time;
    uint64_t htlc_maximum_msat;
};


struct group_update {
    uint64_t time;
    uint64_t group_cap;
    uint64_t* edge_balances;
    long fake_balance_updated_edge_id; // if not -1, it means that the group cap is updated with a fake value and this is the edge id that triggered the update
    uint64_t fake_balance_updated_edge_actual_balance;

    /**
     * triggered_edge_id is the edge that triggered this group update.
     * Payments that are forwarded through this edge will be recorded in the group history.
     * if -1, it means that this group update is not triggered by an edge update, but by a group construction
     */
    long triggered_edge_id;
};


struct group {
    long id;
    struct array* edges;
    uint64_t max_cap_limit;
    uint64_t min_cap_limit;
    uint64_t group_cap;
    uint64_t is_closed; // if not zero, it describes closed time
    uint64_t constructed_time;
    struct element* history; // list of `struct group_update`
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

void open_channel(struct network* network, gsl_rng* random_generator, struct network_params net_params);

struct network* initialize_network(struct network_params net_params, gsl_rng* random_generator);

int update_group(struct group* group, struct network_params net_params, uint64_t current_time, gsl_rng* random_generator, int enable_fake_balance_update, struct edge* triggered_edge);

long get_edge_balance(struct edge* e);

void free_network(struct network* network);

struct edge_snapshot* take_edge_snapshot(struct edge* e, uint64_t sent_amt, short is_in_group, uint64_t group_cap);

#endif
