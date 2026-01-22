#ifndef PAYMENTS_H
#define PAYMENTS_H

#include <stdint.h>
#include <gsl/gsl_rng.h>
#include "array.h"
#include "heap.h"
#include "cloth.h"
#include "network.h"
#include "routing.h"

enum payment_error_type{
  NOERROR,
  NOBALANCE,
  OFFLINENODE, //it corresponds to `FailUnknownNextPeer` in lnd
};

/* register an eventual error occurred when the payment traversed a hop */
struct payment_error{
  enum payment_error_type type;
  struct route_hop* hop;
};

struct payment {
  long id;
  long sender;
  long receiver;
  uint64_t amount; //millisatoshis (target amount for parent, shard amount for child)
  uint64_t max_fee_limit; //millisatoshis
  struct route* route;
  uint64_t start_time;
  uint64_t end_time;
  int attempts;
  struct payment_error error;
  /* attributes for multi-path-payment (mpp)*/
  unsigned int is_shard;
  long parent_id; // -1 if this is a parent payment, else parent's payment id
  struct element* child_shards; // parent only: list of child payment ids (long*)
  uint32_t num_shards_pending; // parent only: number of shards still in-flight
  uint32_t num_shards_succeeded; // parent only: number of shards that succeeded
  uint32_t num_shards_failed; // parent only: number of shards that failed
  uint32_t total_shards_created; // parent only: total number of shards created
  /* attributes used for computing stats */
  unsigned int is_success;
  int offline_node_count;
  int no_balance_count;
  unsigned int is_timeout;
  struct element* history; // list of `struct attempt`
};

struct attempt {
  int attempts;
  uint64_t end_time;
  long error_edge_id;
  enum payment_error_type error_type;
  struct array* route; // array of `struct edge_snapshot`
  short is_succeeded;
  // MPP shard information
  short is_split;              // 1 if this attempt resulted in splitting
  uint64_t split_amount;       // Amount that was split (0 if not split)
  long child_shard1_id;        // First child shard ID (-1 if not split)
  long child_shard2_id;        // Second child shard ID (-1 if not split)
};

struct payment* new_payment(long id, long sender, long receiver, uint64_t amount, uint64_t start_time, uint64_t max_fee_limit);
struct array* initialize_payments(struct payments_params pay_params, long n_nodes, gsl_rng* random_generator);
void add_attempt_history(struct payment* pmt, struct network* network, uint64_t time, short is_succeeded);
void add_split_attempt_history(struct payment* pmt, struct network* network, uint64_t time, uint64_t split_amount, long child1_id, long child2_id);

#endif
