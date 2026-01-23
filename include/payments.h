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
  uint64_t amount; //millisatoshis
  uint64_t max_fee_limit; //millisatoshis
  struct route* route;
  uint64_t start_time;
  uint64_t end_time;
  int attempts;
  struct payment_error error;
  /* attributes for multi-path-payment (mpp)*/
  unsigned int is_shard;
  long parent_payment_id;          // -1 if root, else parent ID
  struct element* child_shard_ids; // List of long* (child IDs), replaces shards_id[2]
  unsigned int split_depth;        // 0=original, 1=first split, etc.
  long pending_shards_count;       // Number of incomplete child shards
  unsigned int is_complete;        // 1 when this payment finished (success or fail)
  struct array* locked_route_hops; // Array of route_hop* locked by this payment
  unsigned int is_rolled_back;     // Flag to prevent double rollback
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
  /* MPP split information */
  short split_occurred;           // 1 if payment was split into shards
  long child_shard_id1;           // First child shard ID (-1 if not split)
  long child_shard_id2;           // Second child shard ID (-1 if not split)
  uint64_t child_shard_amount1;   // First child shard amount
  uint64_t child_shard_amount2;   // Second child shard amount
  unsigned int split_depth;       // Recursion depth at time of this attempt
  char split_reason[256];         // Reason for split (e.g., "no_path_found")
};

struct payment* new_payment(long id, long sender, long receiver, uint64_t amount, uint64_t start_time, uint64_t max_fee_limit);
struct array* initialize_payments(struct payments_params pay_params, long n_nodes, gsl_rng* random_generator);
void add_attempt_history(struct payment* pmt, struct network* network, uint64_t time, short is_succeeded);
void add_split_history(struct payment* pmt, uint64_t time, long child1_id, long child2_id, uint64_t child1_amount, uint64_t child2_amount, const char* reason);
void add_failure_history(struct payment* pmt, uint64_t time, const char* reason);

#endif
