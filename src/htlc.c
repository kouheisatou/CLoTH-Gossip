#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_math.h>

#include "../include/htlc.h"
#include "../include/array.h"
#include "../include/heap.h"
#include "../include/payments.h"
#include "../include/routing.h"
#include "../include/network.h"
#include "../include/event.h"
#include "../include/utils.h"

/* Functions in this file simulate the HTLC mechanism for exchanging payments, as implemented in the Lightning Network.
   They are a (high-level) copy of functions in lnd-v0.9.1-beta (see files `routing/missioncontrol.go`, `htlcswitch/switch.go`, `htlcswitch/link.go`) */


/* AUXILIARY FUNCTIONS */

/* compute the fees to be paid to a hop for forwarding the payment */
uint64_t compute_fee(uint64_t amount_to_forward, struct policy policy) {
  uint64_t fee;
  fee = (policy.fee_proportional*amount_to_forward) / 1000000;
  return policy.fee_base + fee;
}

/* check whether there is sufficient balance in an edge for forwarding the payment; check also that the policies in the edge are respected */
unsigned int check_balance_and_policy(struct edge* edge, struct edge* prev_edge, struct route_hop* prev_hop, struct route_hop* next_hop) {
  uint64_t expected_fee;

  if(next_hop->amount_to_forward > edge->balance)
    return 0;

  if(next_hop->amount_to_forward < edge->policy.min_htlc){
    fprintf(stderr, "ERROR: policy.min_htlc not respected\n");
    exit(-1);
  }

  expected_fee = compute_fee(next_hop->amount_to_forward, edge->policy);
  if(prev_hop->amount_to_forward != next_hop->amount_to_forward + expected_fee){
    fprintf(stderr, "ERROR: policy.fee not respected\n");
    exit(-1);
  }

  if(prev_hop->timelock != next_hop->timelock + prev_edge->policy.timelock){
    fprintf(stderr, "ERROR: policy.timelock not respected\n");
    exit(-1);
  }

  return 1;
}

/* retrieve a hop from a payment route */
struct route_hop *get_route_hop(long node_id, struct array *route_hops, int is_sender) {
  struct route_hop *route_hop;
  long i, index = -1;

  for (i = 0; i < array_len(route_hops); i++) {
    route_hop = array_get(route_hops, i);
    if (is_sender && route_hop->from_node_id == node_id) {
      index = i;
      break;
    }
    if (!is_sender && route_hop->to_node_id == node_id) {
      index = i;
      break;
    }
  }

  if (index == -1)
    return NULL;

  return array_get(route_hops, index);
}


/* FUNCTIONS MANAGING NODE PAIR RESULTS */

/* set the result of a node pair as success: it means that a payment was successfully forwarded in an edge connecting the two nodes of the node pair.
 This information is used by the sender node to find a route that maximizes the possibilities of successfully sending a payment */
void set_node_pair_result_success(struct element** results, long from_node_id, long to_node_id, uint64_t success_amount, uint64_t success_time){
  struct node_pair_result* result;

  result = get_by_key(results[from_node_id], to_node_id, is_equal_key_result);

  if(result == NULL){
    result = malloc(sizeof(struct node_pair_result));
    result->to_node_id = to_node_id;
    result->fail_time = 0;
    result->fail_amount = 0;
    result->success_time = 0;
    result->success_amount = 0;
    results[from_node_id] = push(results[from_node_id], result);
  }

  result->success_time = success_time;
  if(success_amount > result->success_amount)
    result->success_amount = success_amount;
  if(result->fail_time != 0 && result->success_amount > result->fail_amount)
    result->fail_amount = success_amount + 1;
}

/* set the result of a node pair as success: it means that a payment failed when passing through  an edge connecting the two nodes of the node pair.
   This information is used by the sender node to find a route that maximizes the possibilities of successfully sending a payment */
void set_node_pair_result_fail(struct element** results, long from_node_id, long to_node_id, uint64_t fail_amount, uint64_t fail_time){
  struct node_pair_result* result;

  result = get_by_key(results[from_node_id], to_node_id, is_equal_key_result);

  if(result != NULL)
    if(fail_amount > result->fail_amount && fail_time - result->fail_time < 60000)
      return;

  if(result == NULL){
    result = malloc(sizeof(struct node_pair_result));
    result->to_node_id = to_node_id;
    result->fail_time = 0;
    result->fail_amount = 0;
    result->success_time = 0;
    results[from_node_id] = push(results[from_node_id], result);
  }

  result->fail_amount = fail_amount;
  result->fail_time = fail_time;
  if(fail_amount == 0)
    result->success_amount = 0;
  else if(fail_amount != 0 && fail_amount <= result->success_amount)
    result->success_amount = fail_amount - 1;
}

/* process a payment which succeeded */
void process_success_result(struct node* node, struct payment *payment, uint64_t current_time){
  struct route_hop* hop;
  int i;
  struct array* route_hops;
  route_hops = payment->route->route_hops;
  for(i=0; i<array_len(route_hops); i++){
    hop = array_get(route_hops, i);
    set_node_pair_result_success(node->results, hop->from_node_id, hop->to_node_id, hop->amount_to_forward, current_time);
  }
}

/* process a payment which failed (different processments depending on the error type) */
void process_fail_result(struct node* node, struct payment *payment, uint64_t current_time){
  struct route_hop* hop, *error_hop;
  int i;
  struct array* route_hops;

  error_hop = payment->error.hop;

  if(error_hop->from_node_id == payment->sender) //do nothing if the error was originated by the sender (see `processPaymentOutcomeSelf` in lnd)
    return;

  if(payment->error.type == OFFLINENODE) {
    set_node_pair_result_fail(node->results, error_hop->from_node_id, error_hop->to_node_id, 0, current_time);
    set_node_pair_result_fail(node->results, error_hop->to_node_id, error_hop->from_node_id, 0, current_time);
  }
  else if(payment->error.type == NOBALANCE) {
    route_hops = payment->route->route_hops;
    for(i=0; i<array_len(route_hops); i++){
      hop = array_get(route_hops, i);
      if(hop->edge_id == error_hop->edge_id) {
        set_node_pair_result_fail(node->results, hop->from_node_id, hop->to_node_id, hop->amount_to_forward, current_time);
        break;
      }
      set_node_pair_result_success(node->results, hop->from_node_id, hop->to_node_id, hop->amount_to_forward, current_time);
    }
  }
}


void generate_send_payment_event(struct payment* payment, struct array* path, struct simulation* simulation, struct network* network){
  struct route* route;
  uint64_t next_event_time;
  struct event* send_payment_event;
  route = transform_path_into_route(path, payment->amount, network, simulation->current_time);
  payment->route = route;
  // execute send_payment event immediately
  next_event_time = simulation->current_time;
  send_payment_event = new_event(next_event_time, SENDPAYMENT, payment->sender, payment );
  simulation->events = heap_insert(simulation->events, send_payment_event, compare_event);
}


struct payment* create_payment_shard(long shard_id, uint64_t shard_amount, struct payment* payment, struct payment* root_payment){
  struct payment* shard;
  // For fee limit, divide proportionally to amount
  uint64_t shard_fee_limit = payment->max_fee_limit;
  if(payment->max_fee_limit != UINT64_MAX && payment->amount > 0) {
    shard_fee_limit = (uint64_t)((double)payment->max_fee_limit * ((double)shard_amount / (double)payment->amount));
    if(shard_fee_limit == 0) shard_fee_limit = 1; // minimum fee limit
  }
  shard = new_payment(shard_id, payment->sender, payment->receiver, shard_amount, payment->start_time, shard_fee_limit);
  shard->attempts = 0;  // will be incremented on first find_path
  shard->is_shard = 1;
  shard->parent_id = payment->id;
  shard->root_payment_id = root_payment->id;
  return shard;
}

/*HTLC FUNCTIONS*/

// Helper function to get root payment and count shards
static int count_total_shards(struct payment* root_payment) {
  int count = 0;
  if(root_payment->shards_id[0] != -1) count++;
  if(root_payment->shards_id[1] != -1) count++;
  return root_payment->shard_count;
}

// Helper function to check if we can create more shards
static int can_create_more_shards(struct payment* root_payment, int max_shard_count) {
  return root_payment->shard_count < max_shard_count;
}

// Minimum shard size (1000 millisatoshi = 1 satoshi)
#define MIN_SHARD_SIZE 1000000

// Structure for path with capacity and fee info (for GCB optimal N-split)
struct path_info {
  struct array* path;
  uint64_t capacity;
  uint64_t fee;
  uint64_t min_htlc;  // Maximum min_htlc among all edges in the path
};

// Calculate the minimum acceptable amount for a path (max min_htlc of all edges)
static uint64_t calculate_path_min_htlc(struct array* path, struct network* network) {
  uint64_t max_min_htlc = 0;
  for(int i = 0; i < array_len(path); i++) {
    struct path_hop* hop = array_get(path, i);
    struct edge* edge = array_get(network->edges, hop->edge);
    if(edge->policy.min_htlc > max_min_htlc) {
      max_min_htlc = edge->policy.min_htlc;
    }
  }
  return max_min_htlc;
}

// Compare function for sorting paths by fee (ascending)
static int compare_path_info_by_fee(const void* a, const void* b) {
  struct path_info* pa = *(struct path_info**)a;
  struct path_info* pb = *(struct path_info**)b;
  if(pa->fee < pb->fee) return -1;
  if(pa->fee > pb->fee) return 1;
  return 0;
}

// Calculate path capacity using GCB group_cap
static uint64_t calculate_path_capacity_gcb(struct array* path, struct network* network, int first_edge_use_balance) {
  uint64_t min_cap = UINT64_MAX;
  for(int i = 0; i < array_len(path); i++) {
    struct path_hop* hop = array_get(path, i);
    struct edge* edge = array_get(network->edges, hop->edge);
    uint64_t estimated_cap;
    
    if(i == 0 && first_edge_use_balance) {
      // First edge (directly connected): use actual balance
      estimated_cap = edge->balance;
    } else if(edge->group != NULL) {
      // GCB group: use group_cap
      estimated_cap = edge->group->group_cap;
    } else {
      // Non-group edge: use channel_capacity / 2 (conservative estimate)
      struct channel* channel = array_get(network->channels, edge->channel_id);
      estimated_cap = channel->capacity / 2;
    }
    
    if(estimated_cap < min_cap) min_cap = estimated_cap;
  }
  return min_cap;
}

// Find multiple paths for GCB optimal N-split
// Returns array of path_info structures, sorted by fee
static struct array* find_multiple_paths_gcb(
    long sender, long receiver, uint64_t total_amount,
    struct network* network, uint64_t current_time,
    enum routing_method routing_method, uint64_t max_fee_limit,
    int max_paths) {

  struct array* path_infos = array_initialize(max_paths);
  struct element* exclude_edges = NULL;
  uint64_t remaining = total_amount;

  printf("[MPP DEBUG] find_multiple_paths_gcb: sender=%ld, receiver=%ld, total_amount=%llu, max_paths=%d\n",
         sender, receiver, total_amount, max_paths);

  // Log sender balance info
  struct node* sender_node = array_get(network->nodes, sender);
  uint64_t max_bal = 0, total_bal = 0;
  for(int k = 0; k < array_len(sender_node->open_edges); k++) {
    struct edge* e = array_get(sender_node->open_edges, k);
    total_bal += e->balance;
    if(e->balance > max_bal) max_bal = e->balance;
  }
  printf("[MPP DEBUG]   sender_node=%ld, open_edges=%ld, max_balance=%llu, total_balance=%llu\n",
         sender, array_len(sender_node->open_edges), max_bal, total_bal);

  for(int i = 0; i < max_paths && remaining > 0; i++) {
    enum pathfind_error error;
    // GCB MPP: Always search with MIN_SHARD_SIZE first to find a valid path,
    // then use the path's full capacity (from group_cap).
    // This implements "use paths to their full capacity, sorted by fee" strategy.
    uint64_t search_amount = MIN_SHARD_SIZE;
    struct array* path = dijkstra(sender, receiver, search_amount, network, current_time, 0,
                                   &error, routing_method, exclude_edges, max_fee_limit);

    if(path == NULL) {
      printf("[MPP DEBUG]   dijkstra[%d] FAILED: search_amount=%llu, error=%d\n", i, search_amount, error);
      break;
    }

    // Calculate path capacity
    uint64_t capacity = calculate_path_capacity_gcb(path, network, 1);

    // Calculate minimum HTLC for the path
    uint64_t path_min_htlc = calculate_path_min_htlc(path, network);

    // Identify bottleneck edge for logging
    long bottleneck_edge_id = -1;
    uint64_t bottleneck_cap = UINT64_MAX;
    for(int j = 0; j < array_len(path); j++) {
      struct path_hop* hop = array_get(path, j);
      struct edge* edge = array_get(network->edges, hop->edge);
      uint64_t est_cap;
      if(j == 0) est_cap = edge->balance;
      else if(edge->group != NULL) est_cap = edge->group->group_cap;
      else { struct channel* ch = array_get(network->channels, edge->channel_id); est_cap = ch->capacity / 2; }
      if(est_cap < bottleneck_cap) { bottleneck_cap = est_cap; bottleneck_edge_id = edge->id; }
    }

    // Calculate fee for the full capacity amount (not search_amount)
    struct route* route = transform_path_into_route(path, capacity, network, current_time);
    uint64_t fee = route->total_fee;
    free_route(route);

    printf("[MPP DEBUG]   dijkstra[%d] SUCCESS: search_amount=%llu, path_capacity=%llu, fee=%llu, min_htlc=%llu, bottleneck_edge=%ld\n",
           i, search_amount, capacity, fee, path_min_htlc, bottleneck_edge_id);

    // The capacity we can actually use is the minimum edge capacity minus the fee
    // because (amount + fee) must fit within the edge capacity
    if(capacity > fee) {
      capacity = capacity - fee;
    } else {
      // Fee would consume all capacity - skip this path
      printf("[MPP DEBUG]   dijkstra[%d] SKIP: fee(%llu) >= capacity(%llu)\n", i, fee, capacity);
      struct path_hop* first_hop = array_get(path, 0);
      struct edge* first_edge = array_get(network->edges, first_hop->edge);
      exclude_edges = push(exclude_edges, first_edge);
      continue;
    }

    // Create path_info
    struct path_info* info = malloc(sizeof(struct path_info));
    info->path = path;
    info->capacity = capacity;
    info->fee = fee;
    info->min_htlc = path_min_htlc;
    path_infos = array_insert(path_infos, info);

    // Exclude the first edge of this path to find paths through different first edges.
    // This is the primary strategy for finding diverse paths.
    struct path_hop* first_hop = array_get(path, 0);
    struct edge* first_edge = array_get(network->edges, first_hop->edge);
    exclude_edges = push(exclude_edges, first_edge);

    remaining -= (capacity < remaining) ? capacity : remaining;
    printf("[MPP DEBUG]   path[%d] added: usable_capacity=%llu, remaining=%llu, bottleneck_edge=%ld\n",
           i, capacity, remaining, bottleneck_edge_id);
  }

  // If we still have remaining amount and found at least 1 path,
  // try a second pass: exclude bottleneck edges instead of first edges
  // to find alternative routes through the same first edge but different intermediate nodes.
  if(remaining > 0 && array_len(path_infos) > 0) {
    list_free(exclude_edges);
    exclude_edges = NULL;

    printf("[MPP DEBUG]   second pass: remaining=%llu, excluding bottleneck edges\n", remaining);

    // Exclude bottleneck edges from all found paths
    for(int j = 0; j < array_len(path_infos); j++) {
      struct path_info* info = array_get(path_infos, j);
      // Find bottleneck edge in this path
      uint64_t min_cap = UINT64_MAX;
      long bn_id = -1;
      for(int k = 0; k < array_len(info->path); k++) {
        struct path_hop* hop = array_get(info->path, k);
        struct edge* edge = array_get(network->edges, hop->edge);
        uint64_t est_cap;
        if(k == 0) est_cap = edge->balance;
        else if(edge->group != NULL) est_cap = edge->group->group_cap;
        else { struct channel* ch = array_get(network->channels, edge->channel_id); est_cap = ch->capacity / 2; }
        if(est_cap < min_cap) { min_cap = est_cap; bn_id = edge->id; }
      }
      if(bn_id != -1) {
        struct edge* bn_edge = array_get(network->edges, bn_id);
        // Only exclude if not already in list
        if(!is_in_list(exclude_edges, &(bn_edge->id), is_equal_edge)) {
          exclude_edges = push(exclude_edges, bn_edge);
        }
      }
    }

    for(int i = array_len(path_infos); i < max_paths && remaining > 0; i++) {
      enum pathfind_error error;
      // GCB MPP: Always search with MIN_SHARD_SIZE first to find a valid path
      uint64_t search_amount = MIN_SHARD_SIZE;
      struct array* path = dijkstra(sender, receiver, search_amount, network, current_time, 0,
                                     &error, routing_method, exclude_edges, max_fee_limit);
      if(path == NULL) {
        printf("[MPP DEBUG]   dijkstra_pass2[%d] FAILED: search_amount=%llu, error=%d\n", i, search_amount, error);
        break;
      }

      uint64_t capacity = calculate_path_capacity_gcb(path, network, 1);

      // Calculate minimum HTLC for the path
      uint64_t path_min_htlc = calculate_path_min_htlc(path, network);

      struct route* route = transform_path_into_route(path, capacity, network, current_time);
      uint64_t fee = route->total_fee;
      free_route(route);

      printf("[MPP DEBUG]   dijkstra_pass2[%d] SUCCESS: search_amount=%llu, path_capacity=%llu, fee=%llu, min_htlc=%llu\n",
             i, search_amount, capacity, fee, path_min_htlc);

      if(capacity > fee) {
        capacity = capacity - fee;
      } else {
        printf("[MPP DEBUG]   dijkstra_pass2[%d] SKIP: fee(%llu) >= capacity(%llu)\n", i, fee, capacity);
        // Exclude first edge and try again
        struct path_hop* fhop = array_get(path, 0);
        struct edge* fedge = array_get(network->edges, fhop->edge);
        exclude_edges = push(exclude_edges, fedge);
        continue;
      }

      struct path_info* info = malloc(sizeof(struct path_info));
      info->path = path;
      info->capacity = capacity;
      info->fee = fee;
      info->min_htlc = path_min_htlc;
      path_infos = array_insert(path_infos, info);

      // In pass 2, exclude the first edge to find yet more diverse paths
      struct path_hop* first_hop = array_get(path, 0);
      struct edge* first_edge = array_get(network->edges, first_hop->edge);
      exclude_edges = push(exclude_edges, first_edge);

      remaining -= (capacity < remaining) ? capacity : remaining;
      printf("[MPP DEBUG]   path_pass2[%d] added: usable_capacity=%llu, remaining=%llu\n", i, capacity, remaining);
    }
  }

  list_free(exclude_edges);

  printf("[MPP DEBUG]   find_multiple_paths_gcb result: found %ld paths\n", array_len(path_infos));

  // Sort by fee (ascending)
  if(array_len(path_infos) > 1) {
    // Simple bubble sort (paths count is small)
    for(int i = 0; i < array_len(path_infos) - 1; i++) {
      for(int j = 0; j < array_len(path_infos) - i - 1; j++) {
        struct path_info* a = array_get(path_infos, j);
        struct path_info* b = array_get(path_infos, j + 1);
        if(a->fee > b->fee) {
          // Swap
          path_infos->element[j] = b;
          path_infos->element[j + 1] = a;
        }
      }
    }
  }

  return path_infos;
}

// Free path_info array
static void free_path_infos(struct array* path_infos) {
  for(int i = 0; i < array_len(path_infos); i++) {
    struct path_info* info = array_get(path_infos, i);
    // Note: don't free path here as it may be used by shards
    free(info);
  }
  array_free(path_infos);
}

/* find a path for a payment (a modified version of dijkstra is used: see `routing.c`) */
void find_path(struct event *event, struct simulation* simulation, struct network* network, struct array** payments, struct payments_params pay_params, struct network_params net_params) {
  struct payment *payment, *shard1, *shard2, *root_payment;
  struct array *path;
  uint64_t shard1_amount, shard2_amount;
  enum pathfind_error error;
  long shard1_id, shard2_id;
  unsigned int mpp = pay_params.mpp;
  enum routing_method routing_method = net_params.routing_method;

  payment = event->payment;
  
  // Get root payment for shard tracking
  root_payment = array_get(*payments, payment->root_payment_id);

  ++(payment->attempts);

  if(net_params.payment_timeout != -1 && simulation->current_time > payment->start_time + net_params.payment_timeout) {
    payment->end_time = simulation->current_time;
    payment->is_timeout = 1;
    printf("[MPP DEBUG] TIMEOUT: payment_id=%ld, is_shard=%u, elapsed=%llu\n", 
           payment->id, payment->is_shard, simulation->current_time - payment->start_time);
    return;
  }

  // find path
  if(routing_method == CLOTH_ORIGINAL) {
      if (payment->attempts == 1 && !payment->is_shard) {
          path = paths[payment->id];
      }else {
          path = dijkstra(payment->sender, payment->receiver, payment->amount, network, simulation->current_time, 0, &error, net_params.routing_method, NULL, payment->max_fee_limit);
      }
  } else {

      if (payment->attempts == 1 && !payment->is_shard) {
          path = paths[payment->id];
          if (path != NULL) {

              // calc path capacity
              uint64_t path_cap = INT64_MAX;
              for (int i = 0; i < array_len(path); i++) {
                  struct path_hop *hop = array_get(path, i);
                  struct edge *edge = array_get(network->edges, hop->edge);
                  uint64_t estimated_cap;
                  if (i == 0) {
                      // if first edge of the path (directory connected edge to source node)
                      estimated_cap = edge->balance;
                  } else {
                      estimated_cap = estimate_capacity(edge, network, routing_method);
                  }
                  if (estimated_cap < path_cap) path_cap = estimated_cap;
              }

              // calc total fee
              struct route *route = transform_path_into_route(path, payment->amount, network, simulation->current_time);
              uint64_t fee = route->total_fee;
              free_route(route);

              // if path capacity is not enough to send the payment, find new path
              if (path_cap < payment->amount + fee) {
                  path = dijkstra(payment->sender, payment->receiver, payment->amount, network, simulation->current_time, 0, &error, net_params.routing_method, NULL, payment->max_fee_limit);
              }
          } else {
              path = dijkstra(payment->sender, payment->receiver, payment->amount, network, simulation->current_time, 0, &error, net_params.routing_method, NULL, payment->max_fee_limit);
          }
      } else {

          // exclude edges from failed attempts
          struct element* exclude_edges = NULL;
          for(struct element* iterator = payment->history; iterator != NULL; iterator = iterator->next) {
            struct attempt* a = iterator->data;
            if(a->error_edge_id != 0) {
              struct edge* exclude_edge = array_get(network->edges, a->error_edge_id);
              exclude_edges = push(exclude_edges, exclude_edge);
            }
          }

          path = dijkstra(payment->sender, payment->receiver, payment->amount, network, simulation->current_time, 0, &error, net_params.routing_method, exclude_edges, payment->max_fee_limit);
          list_free(exclude_edges);
      }
  }

  if (path != NULL) {
    printf("[MPP DEBUG] PATH_FOUND: payment_id=%ld, is_shard=%u, amount=%llu, attempts=%d\n",
           payment->id, payment->is_shard, payment->amount, payment->attempts);
    generate_send_payment_event(payment, path, simulation, network);
    return;
  }

  // No path found - try MPP if conditions are met
  printf("[MPP DEBUG] NOPATH: payment_id=%ld, is_shard=%u, amount=%llu, attempts=%d, error=%d\n",
         payment->id, payment->is_shard, payment->amount, payment->attempts, error);

  if(mpp && path == NULL && payment->amount >= MIN_SHARD_SIZE * 2) {
    
    // Mark that MPP was triggered
    if(!payment->mpp_triggered) {
      payment->mpp_triggered = 1;
      root_payment->mpp_triggered = 1;
    }

    // Section 4: GCB optimal N-split for GROUP_ROUTING and GROUP_ROUTING_CUL
    // This applies to both root payments AND shards (grandchildren etc.)
    if(routing_method == GROUP_ROUTING || routing_method == GROUP_ROUTING_CUL) {
      
      // Find multiple paths sorted by fee
      int max_paths = pay_params.max_shard_count - root_payment->shard_count;
      if(max_paths < 1) max_paths = 1;
      if(max_paths > 16) max_paths = 16;  // Reasonable limit for path search
      
      struct array* path_infos = find_multiple_paths_gcb(
          payment->sender, payment->receiver, payment->amount,
          network, simulation->current_time, routing_method,
          payment->max_fee_limit, max_paths);
      
      if(array_len(path_infos) == 0) {
        printf("[MPP DEBUG] GCB_NSPLIT_NO_PATHS: payment_id=%ld, falling back to recursive 2-split\n", payment->id);
        free_path_infos(path_infos);
        // Fall through to recursive 2-split below
        goto recursive_2_split;
      }
      
      // Greedily allocate payment amount to paths (sorted by fee, cheapest first)
      uint64_t remaining = payment->amount;
      int shard_count = 0;
      struct array* shard_amounts = array_initialize(array_len(path_infos));
      struct array* shard_paths = array_initialize(array_len(path_infos));
      
      for(int i = 0; i < array_len(path_infos) && remaining > 0; i++) {
        struct path_info* info = array_get(path_infos, i);

        // Capacity already has fee subtracted in find_multiple_paths_gcb
        uint64_t alloc = (info->capacity < remaining) ? info->capacity : remaining;

        // Ensure minimum shard size and respect min_htlc
        uint64_t min_acceptable = (info->min_htlc > MIN_SHARD_SIZE) ? info->min_htlc : MIN_SHARD_SIZE;

        if(alloc < min_acceptable && remaining >= min_acceptable) {
          continue;  // Skip paths with too little capacity
        }
        if(alloc < min_acceptable) {
          // Last shard - only take it if it meets min_htlc requirement
          if(remaining >= info->min_htlc) {
            alloc = remaining;
          } else {
            // Cannot send remaining amount on this path due to min_htlc constraint
            printf("[MPP DEBUG]   path[%d] SKIP: remaining=%llu < min_htlc=%llu\n", i, remaining, info->min_htlc);
            continue;
          }
        }

        uint64_t* amount_ptr = malloc(sizeof(uint64_t));
        *amount_ptr = alloc;
        shard_amounts = array_insert(shard_amounts, amount_ptr);
        shard_paths = array_insert(shard_paths, info->path);

        remaining -= alloc;
        shard_count++;
      }
      
      // Check if we can cover the full amount
      if(remaining > 0) {
        // GCB MPP: If we found paths but can't cover full amount,
        // create shards for found paths and one additional shard for remaining.
        // The remaining shard will try to find its own path via FINDPATH.
        // IMPORTANT: Only create remaining shard if it's >= MIN_SHARD_SIZE to avoid min_htlc violations.
        if(shard_count > 0 && remaining >= MIN_SHARD_SIZE) {
          // Check shard count limit (need slots for found paths + 1 for remaining)
          if(root_payment->shard_count + shard_count + 1 > pay_params.max_shard_count) {
            printf("[MPP DEBUG] FAIL: payment_id=%ld, reason=MAX_SHARD_COUNT_REACHED, needed=%d, max=%d\n",
                   payment->id, root_payment->shard_count + shard_count + 1, pay_params.max_shard_count);
            for(int i = 0; i < array_len(shard_amounts); i++) {
              free(array_get(shard_amounts, i));
            }
            array_free(shard_amounts);
            array_free(shard_paths);
            free_path_infos(path_infos);
            payment->end_time = simulation->current_time;
            return;
          }

          printf("[MPP DEBUG] GCB_PARTIAL_SPLIT: payment_id=%ld, amount=%llu, n_shards_with_path=%d, remaining=%llu\n",
                 payment->id, payment->amount, shard_count, remaining);
          
          long first_shard_id = array_len(*payments);
          
          // Create shards for found paths (schedule via FINDPATH to properly check balances)
          for(int i = 0; i < shard_count; i++) {
            uint64_t* amount_ptr = array_get(shard_amounts, i);
            uint64_t shard_amt = *amount_ptr;
            // Note: We don't use the pre-found path directly anymore.
            // Instead, we let each shard find its own path via FINDPATH.
            // This ensures proper balance checking at send time.
            
            long shard_id = array_len(*payments);
            struct payment* shard = create_payment_shard(shard_id, shard_amt, payment, root_payment);
            *payments = array_insert(*payments, shard);
            
            printf("[MPP DEBUG]   SHARD_GCB: shard_id=%ld, amount=%llu\n",
                   shard_id, shard_amt);
            
            // Schedule FINDPATH event (not direct send) so balance is checked at send time
            struct event* shard_event = new_event(simulation->current_time, FINDPATH, shard->sender, shard);
            simulation->events = heap_insert(simulation->events, shard_event, compare_event);
            
            free(amount_ptr);
          }
          
          // Create one shard for remaining amount (will find its own path via FINDPATH)
          long remaining_shard_id = array_len(*payments);
          struct payment* remaining_shard = create_payment_shard(remaining_shard_id, remaining, payment, root_payment);
          *payments = array_insert(*payments, remaining_shard);
          
          printf("[MPP DEBUG]   SHARD_REMAINING: shard_id=%ld, amount=%llu\n",
                 remaining_shard_id, remaining);
          
          // Schedule FINDPATH for remaining shard
          struct event* remaining_event = new_event(simulation->current_time, FINDPATH, remaining_shard->sender, remaining_shard);
          simulation->events = heap_insert(simulation->events, remaining_event, compare_event);
          
          // Update root payment shard count
          root_payment->shard_count += shard_count + 1;
          
          // Record in parent's shards
          // shards_id[0] = first shard from found paths
          // shards_id[1] = second shard from found paths OR remaining shard
          payment->shards_id[0] = first_shard_id;
          if(shard_count >= 2) {
            payment->shards_id[1] = first_shard_id + 1;
          } else {
            // Only 1 path found, so shards_id[1] is the remaining shard
            payment->shards_id[1] = remaining_shard_id;
          }
          
          add_split_history(payment, simulation->current_time, 
                            first_shard_id, 
                            shard_count >= 2 ? first_shard_id + 1 : remaining_shard_id);
          
          array_free(shard_amounts);
          array_free(shard_paths);
          free_path_infos(path_infos);
          return;
        }

        // If shard_count > 0 but remaining < MIN_SHARD_SIZE, we can't create a valid remaining shard.
        // Fall back to recursive 2-split.
        if(shard_count > 0 && remaining > 0 && remaining < MIN_SHARD_SIZE) {
          printf("[MPP DEBUG] GCB_REMAINING_TOO_SMALL: payment_id=%ld, remaining=%llu < MIN_SHARD_SIZE=%d, falling back to recursive 2-split\n",
                 payment->id, remaining, MIN_SHARD_SIZE);
          // Free allocated amounts
          for(int i = 0; i < array_len(shard_amounts); i++) {
            free(array_get(shard_amounts, i));
          }
          array_free(shard_amounts);
          array_free(shard_paths);
          free_path_infos(path_infos);
          goto recursive_2_split;
        }

        printf("[MPP DEBUG] GCB_NSPLIT_INSUFFICIENT: payment_id=%ld, remaining=%llu, shard_count=%d, falling back to recursive 2-split\n",
               payment->id, remaining, shard_count);
        // Free allocated amounts
        for(int i = 0; i < array_len(shard_amounts); i++) {
          free(array_get(shard_amounts, i));
        }
        array_free(shard_amounts);
        array_free(shard_paths);
        free_path_infos(path_infos);
        // Fall through to recursive 2-split below
        goto recursive_2_split;
      }
      
      // If only 1 shard and it covers the full amount, this means a single path exists
      // but MPP was triggered (probably due to routing failure). In this case, fall back
      // to 2-split to try sending smaller amounts that are more likely to succeed.
      if(shard_count == 1 && remaining == 0) {
        printf("[MPP DEBUG] GCB_SINGLE_PATH_FALLBACK: payment_id=%ld, amount=%llu, falling back to 2-split\n",
               payment->id, payment->amount);
        
        for(int i = 0; i < array_len(shard_amounts); i++) {
          free(array_get(shard_amounts, i));
        }
        array_free(shard_amounts);
        array_free(shard_paths);
        free_path_infos(path_infos);
        goto recursive_2_split;
      }
      
      // Check shard count limit
      if(root_payment->shard_count + shard_count > pay_params.max_shard_count) {
        printf("[MPP DEBUG] FAIL: payment_id=%ld, reason=MAX_SHARD_COUNT_REACHED, needed=%d, max=%d\n",
               payment->id, root_payment->shard_count + shard_count, pay_params.max_shard_count);
        for(int i = 0; i < array_len(shard_amounts); i++) {
          free(array_get(shard_amounts, i));
        }
        array_free(shard_amounts);
        array_free(shard_paths);
        free_path_infos(path_infos);
        payment->end_time = simulation->current_time;
        return;
      }
      
      // Create N shards (N >= 2 at this point)
      printf("[MPP DEBUG] GCB_SPLIT: payment_id=%ld, amount=%llu, n_shards=%d\n",
             payment->id, payment->amount, shard_count);
      
      long first_shard_id = array_len(*payments);
      for(int i = 0; i < shard_count; i++) {
        uint64_t* amount_ptr = array_get(shard_amounts, i);
        uint64_t shard_amount = *amount_ptr;
        struct array* shard_path = array_get(shard_paths, i);
        
        long shard_id = array_len(*payments);
        struct payment* shard = create_payment_shard(shard_id, shard_amount, payment, root_payment);
        *payments = array_insert(*payments, shard);
        
        printf("[MPP DEBUG]   SHARD: shard_id=%ld, amount=%llu, path_len=%ld\n",
               shard_id, shard_amount, array_len(shard_path));
        
        // Generate send payment event with the pre-found path
        generate_send_payment_event(shard, shard_path, simulation, network);
        
        free(amount_ptr);
      }
      
      // Update root payment shard count
      root_payment->shard_count += shard_count;
      
      // Record in parent's shards (only first 2 for compatibility, but we track all via shard_count)
      if(shard_count >= 1) payment->shards_id[0] = first_shard_id;
      if(shard_count >= 2) payment->shards_id[1] = first_shard_id + 1;
      
      // Record split in attempt history (using first two shard ids for compatibility)
      add_split_history(payment, simulation->current_time, 
                        first_shard_id, 
                        shard_count >= 2 ? first_shard_id + 1 : -1);
      
      array_free(shard_amounts);
      array_free(shard_paths);
      free_path_infos(path_infos);
      return;
    }
    
    // Section 3: Recursive 2-split (used by non-GCB methods, and as fallback for GCB when N-split fails)
    recursive_2_split:
    // Check shard count limit (need at least 2 more slots for new shards)
    if(root_payment->shard_count + 2 > pay_params.max_shard_count) {
      printf("[MPP DEBUG] FAIL: payment_id=%ld, reason=MAX_SHARD_COUNT_REACHED, shard_count=%d, max=%d\n",
             payment->id, root_payment->shard_count, pay_params.max_shard_count);
      payment->end_time = simulation->current_time;
      return;
    }

    // Calculate shard amounts (50/50 split)
    shard1_amount = payment->amount / 2;
    shard2_amount = payment->amount - shard1_amount;
    
    // Check minimum shard size
    if(shard1_amount < MIN_SHARD_SIZE || shard2_amount < MIN_SHARD_SIZE) {
      printf("[MPP DEBUG] FAIL: payment_id=%ld, reason=SHARD_TOO_SMALL, shard1=%llu, shard2=%llu, min=%d\n",
             payment->id, shard1_amount, shard2_amount, MIN_SHARD_SIZE);
      payment->end_time = simulation->current_time;
      return;
    }

    // Create shards and schedule FINDPATH for each
    // This allows recursive splitting - if a shard can't find a path, it will split again
    shard1_id = array_len(*payments);
    shard2_id = array_len(*payments) + 1;
    shard1 = create_payment_shard(shard1_id, shard1_amount, payment, root_payment);
    shard2 = create_payment_shard(shard2_id, shard2_amount, payment, root_payment);
    
    *payments = array_insert(*payments, shard1);
    *payments = array_insert(*payments, shard2);
    
    // Update parent payment (note: is_shard is already set correctly on shards via create_payment_shard)
    payment->shards_id[0] = shard1_id;
    payment->shards_id[1] = shard2_id;
    
    // Update root payment shard count
    root_payment->shard_count += 2;
    
    // Record split in attempt history
    add_split_history(payment, simulation->current_time, shard1_id, shard2_id);
    
    printf("[MPP DEBUG] SPLIT: parent_id=%ld, parent_amount=%llu, shard1_id=%ld, shard1_amount=%llu, shard2_id=%ld, shard2_amount=%llu, total_shards=%d\n",
           payment->id, payment->amount, shard1_id, shard1_amount, shard2_id, shard2_amount, root_payment->shard_count);
    
    // Schedule FINDPATH events for both shards
    struct event* shard1_event = new_event(simulation->current_time, FINDPATH, shard1->sender, shard1);
    struct event* shard2_event = new_event(simulation->current_time, FINDPATH, shard2->sender, shard2);
    simulation->events = heap_insert(simulation->events, shard1_event, compare_event);
    simulation->events = heap_insert(simulation->events, shard2_event, compare_event);
    
    return;
  }

  // no path and MPP conditions not met
  printf("[MPP DEBUG] FAIL: payment_id=%ld, reason=NOPATH, is_shard=%u, amount=%llu, attempts=%d\n",
         payment->id, payment->is_shard, payment->amount, payment->attempts);
  payment->end_time = simulation->current_time;
}

/* send an HTLC for the payment (behavior of the payment sender) */
void send_payment(struct event* event, struct simulation* simulation, struct network* network, struct network_params net_params){
  struct payment* payment;
  uint64_t next_event_time;
  struct route* route;
  struct route_hop* first_route_hop;
  struct edge* next_edge;
  struct event* next_event;
  enum event_type event_type;
  unsigned long is_next_node_offline;
  struct node* node;

  payment = event->payment;
  route = payment->route;
  node = array_get(network->nodes, event->node_id);
  first_route_hop = array_get(route->route_hops, 0);
  next_edge = array_get(network->edges, first_route_hop->edge_id);

  if(!is_present(next_edge->id, node->open_edges)) {
    printf("ERROR (send_payment): edge %ld is not an edge of node %ld \n", next_edge->id, node->id);
    exit(-1);
  }

  first_route_hop->edges_lock_start_time = simulation->current_time;

  /* simulate the case that the next node in the route is offline */
  is_next_node_offline = gsl_ran_discrete(simulation->random_generator, network->faulty_node_prob);
  if(is_next_node_offline){
    payment->offline_node_count += 1;
    payment->error.type = OFFLINENODE;
    payment->error.hop = first_route_hop;
    next_event_time = simulation->current_time + OFFLINELATENCY;
    next_event = new_event(next_event_time, RECEIVEFAIL, event->node_id, event->payment);
    simulation->events = heap_insert(simulation->events, next_event, compare_event);
    return;
  }

  // fail no balance
  if(first_route_hop->amount_to_forward > next_edge->balance) {
    payment->error.type = NOBALANCE;
    payment->error.hop = first_route_hop;
    payment->no_balance_count += 1;
    next_event_time = simulation->current_time;
    next_event = new_event(next_event_time, RECEIVEFAIL, event->node_id, event->payment);
    simulation->events = heap_insert(simulation->events, next_event, compare_event);
    return;
  }

  // update balance
  uint64_t prev_balance = next_edge->balance;
  next_edge->balance -= first_route_hop->amount_to_forward;

  next_edge->tot_flows += 1;

  // success sending
  event_type = first_route_hop->to_node_id == payment->receiver ? RECEIVEPAYMENT : FORWARDPAYMENT;
  next_event_time = simulation->current_time + net_params.average_payment_forward_interval + (long)(fabs(net_params.variance_payment_forward_interval * gsl_ran_ugaussian(simulation->random_generator)));
  next_event = new_event(next_event_time, event_type, first_route_hop->to_node_id, event->payment);
  simulation->events = heap_insert(simulation->events, next_event, compare_event);
}

/* forward an HTLC for the payment (behavior of an intermediate hop node in a route) */
void forward_payment(struct event* event, struct simulation* simulation, struct network* network, struct network_params net_params){
  struct payment* payment;
  struct route* route;
  struct route_hop* next_route_hop, *previous_route_hop;
  long  prev_node_id;
  enum event_type event_type;
  struct event* next_event;
  uint64_t next_event_time;
  unsigned long is_next_node_offline;
  struct node* node;
  unsigned int is_last_hop;
  struct edge *next_edge = NULL, *prev_edge;

  payment = event->payment;
  node = array_get(network->nodes, event->node_id);
  route = payment->route;
  next_route_hop=get_route_hop(node->id, route->route_hops, 1);
  previous_route_hop = get_route_hop(node->id, route->route_hops, 0);
  is_last_hop = next_route_hop->to_node_id == payment->receiver;
    next_route_hop->edges_lock_start_time = simulation->current_time;

  if(!is_present(next_route_hop->edge_id, node->open_edges)) {
    printf("ERROR (forward_payment): edge %ld is not an edge of node %ld \n", next_route_hop->edge_id, node->id);
    exit(-1);
  }

  /* simulate the case that the next node in the route is offline */
  is_next_node_offline = gsl_ran_discrete(simulation->random_generator, network->faulty_node_prob);
  if(is_next_node_offline && !is_last_hop){ //assume that the receiver node is always online
    payment->offline_node_count += 1;
    payment->error.type = OFFLINENODE;
    payment->error.hop = next_route_hop;
    prev_node_id = previous_route_hop->from_node_id;
    event_type = prev_node_id == payment->sender ? RECEIVEFAIL : FORWARDFAIL;
    next_event_time = simulation->current_time + net_params.average_payment_forward_interval + (long)(fabs(net_params.variance_payment_forward_interval * gsl_ran_ugaussian(simulation->random_generator))) + OFFLINELATENCY;
    next_event = new_event(next_event_time, event_type, prev_node_id, event->payment);
    simulation->events = heap_insert(simulation->events, next_event, compare_event);
    return;
  }

  // BEGIN -- NON-STRICT FORWARDING (cannot simulate it because the current blokchain height is needed)
  /* can_send_htlc = 0; */
  /* prev_edge = array_get(network->edges,previous_route_hop->edge_id); */
  /* for(i=0; i<array_len(node->open_edges); i++) { */
  /*   next_edge = array_get(node->open_edges, i); */
  /*   if(next_edge->to_node_id != next_route_hop->to_node_id) continue; */
  /*   can_send_htlc = check_balance_and_policy(next_edge, prev_edge, previous_route_hop, next_route_hop, is_last_hop); */
  /*   if(can_send_htlc) break; */
  /* } */

  /* if(!can_send_htlc){ */
  /*   next_edge = array_get(network->edges,next_route_hop->edge_id); */
  /*   printf("no balance: %ld < %ld\n", next_edge->balance, next_route_hop->amount_to_forward ); */
  /*   printf("prev_hop->timelock, next_hop->timelock: %d, %d + %d\n", previous_route_hop->timelock, next_route_hop->timelock, next_edge->policy.timelock ); */
  /*   payment->error.type = NOBALANCE; */
  /*   payment->error.hop = next_route_hop; */
  /*   payment->no_balance_count += 1; */
  /*   prev_node_id = previous_route_hop->from_node_id; */
  /*   event_type = prev_node_id == payment->sender ? RECEIVEFAIL : FORWARDFAIL; */
  /*   next_event_time = simulation->current_time + 100 + gsl_ran_ugaussian(simulation->random_generator);//prev_channel->latency; */
  /*   next_event = new_event(next_event_time, event_type, prev_node_id, event->payment); */
  /*   simulation->events = heap_insert(simulation->events, next_event, compare_event); */
  /*   return; */
  /* } */

  /* next_route_hop->edge_id = next_edge->id; */
  //END -- NON-STRICT FORWARDING

  // STRICT FORWARDING
  prev_edge = array_get(network->edges,previous_route_hop->edge_id);
  next_edge = array_get(network->edges, next_route_hop->edge_id);

  // fail no balance
  if(!check_balance_and_policy(next_edge, prev_edge, previous_route_hop, next_route_hop)){
    payment->error.type = NOBALANCE;
    payment->error.hop = next_route_hop;
    payment->no_balance_count += 1;
    prev_node_id = previous_route_hop->from_node_id;
    event_type = prev_node_id == payment->sender ? RECEIVEFAIL : FORWARDFAIL;
    next_event_time = simulation->current_time + net_params.average_payment_forward_interval + (long)(fabs(net_params.variance_payment_forward_interval * gsl_ran_ugaussian(simulation->random_generator)));//prev_channel->latency;
    next_event = new_event(next_event_time, event_type, prev_node_id, event->payment);
    simulation->events = heap_insert(simulation->events, next_event, compare_event);
    return;
  }

  // update balance
  uint64_t prev_balance = next_edge->balance;
  next_edge->balance -= next_route_hop->amount_to_forward;

  next_edge->tot_flows += 1;

  // success forwarding
  event_type = is_last_hop  ? RECEIVEPAYMENT : FORWARDPAYMENT;
  // interval for forwarding payment
  next_event_time = simulation->current_time + net_params.average_payment_forward_interval + (long)(fabs(net_params.variance_payment_forward_interval * gsl_ran_ugaussian(simulation->random_generator)));//next_channel->latency;
  next_event = new_event(next_event_time, event_type, next_route_hop->to_node_id, event->payment);
  simulation->events = heap_insert(simulation->events, next_event, compare_event);
}

/* receive a payment (behavior of the payment receiver node) */
void receive_payment(struct event* event, struct simulation* simulation, struct network* network, struct network_params net_params){
  long  prev_node_id;
  struct route* route;
  struct payment* payment;
  struct route_hop* last_route_hop;
  struct edge* forward_edge,*backward_edge;
  struct event* next_event;
  enum event_type event_type;
  uint64_t next_event_time;
  struct node* node;

  payment = event->payment;
  route = payment->route;
  node = array_get(network->nodes, event->node_id);

  last_route_hop = array_get(route->route_hops, array_len(route->route_hops) - 1);
  forward_edge = array_get(network->edges, last_route_hop->edge_id);
  backward_edge = array_get(network->edges, forward_edge->counter_edge_id);

  last_route_hop->edges_lock_end_time = simulation->current_time;

  if(!is_present(backward_edge->id, node->open_edges)) {
    printf("ERROR (receive_payment): edge %ld is not an edge of node %ld \n", backward_edge->id, node->id);
    exit(-1);
  }

  // update balance
  backward_edge->balance += last_route_hop->amount_to_forward;

  payment->is_success = 1;

  prev_node_id = last_route_hop->from_node_id;
  event_type = prev_node_id == payment->sender ? RECEIVESUCCESS : FORWARDSUCCESS;
  next_event_time = simulation->current_time + net_params.average_payment_forward_interval + (long)(fabs(net_params.variance_payment_forward_interval * gsl_ran_ugaussian(simulation->random_generator)));//channel->latency;
  next_event = new_event(next_event_time, event_type, prev_node_id, event->payment);
  simulation->events = heap_insert(simulation->events, next_event, compare_event);
}

/* forward an HTLC success back to the payment sender (behavior of a intermediate hop node in the route) */
void forward_success(struct event* event, struct simulation* simulation, struct network* network, struct network_params net_params){
  struct route_hop* prev_hop;
  struct payment* payment;
  struct edge* forward_edge, * backward_edge;
  long prev_node_id;
  struct event* next_event;
  enum event_type event_type;
  struct node* node;
  uint64_t next_event_time;

  payment = event->payment;
  prev_hop = get_route_hop(event->node_id, payment->route->route_hops, 0);
  forward_edge = array_get(network->edges, prev_hop->edge_id);
  backward_edge = array_get(network->edges, forward_edge->counter_edge_id);
  node = array_get(network->nodes, event->node_id);
  prev_hop->edges_lock_end_time = simulation->current_time;

  if(!is_present(backward_edge->id, node->open_edges)) {
    printf("ERROR (forward_success): edge %ld is not an edge of node %ld \n", backward_edge->id, node->id);
    exit(-1);
  }

  // update balance
  backward_edge->balance += prev_hop->amount_to_forward;

  prev_node_id = prev_hop->from_node_id;
  event_type = prev_node_id == payment->sender ? RECEIVESUCCESS : FORWARDSUCCESS;
  next_event_time = simulation->current_time + net_params.average_payment_forward_interval + (long)(fabs(net_params.variance_payment_forward_interval * gsl_ran_ugaussian(simulation->random_generator)));//prev_channel->latency;
  next_event = new_event(next_event_time, event_type, prev_node_id, event->payment);
  simulation->events = heap_insert(simulation->events, next_event, compare_event);
}

/* receive an HTLC success (behavior of the payment sender node) */
void receive_success(struct event* event, struct simulation* simulation, struct network* network, struct network_params net_params){
  struct node* node;
  struct payment* payment;
  payment = event->payment;
  node = array_get(network->nodes, event->node_id);
  event->payment->end_time = simulation->current_time;

  add_attempt_history(payment, network, simulation->current_time, 1);
  
  // MPP debug logging
  if(payment->is_shard) {
    printf("[MPP DEBUG] SHARD_COMPLETE: shard_id=%ld, parent_id=%ld, root_id=%ld, is_success=1, amount=%llu, fee=%llu\n",
           payment->id, payment->parent_id, payment->root_payment_id, 
           payment->amount, payment->route ? payment->route->total_fee : 0);
  } else if(payment->mpp_triggered) {
    printf("[MPP DEBUG] SUCCESS: payment_id=%ld, shard_count=%d, total_time=%llu\n",
           payment->id, payment->shard_count, simulation->current_time - payment->start_time);
  } else {
    printf("[MPP DEBUG] SUCCESS: payment_id=%ld, amount=%llu, fee=%llu, attempts=%d\n",
           payment->id, payment->amount, payment->route ? payment->route->total_fee : 0, payment->attempts);
  }
  
    // next event
    uint64_t next_event_time = simulation->current_time + net_params.group_broadcast_delay;

    // request_group_update event
    if (net_params.routing_method == GROUP_ROUTING) {
        struct event *next_event = new_event(next_event_time, UPDATEGROUP, event->node_id, event->payment);
        simulation->events = heap_insert(simulation->events, next_event, compare_event);
    }

    // channel update broadcast event
    struct event *channel_update_event = new_event(next_event_time, CHANNELUPDATESUCCESS, node->id, payment);
    simulation->events = heap_insert(simulation->events, channel_update_event, compare_event);
}

/* forward an HTLC fail back to the payment sender (behavior of a intermediate hop node in the route) */
void forward_fail(struct event* event, struct simulation* simulation, struct network* network, struct network_params net_params){
  struct payment* payment;
  struct route_hop* next_hop, *prev_hop;
  struct edge* next_edge;
  long prev_node_id;
  struct event* next_event;
  enum event_type event_type;
  struct node* node;
  uint64_t next_event_time;

  node = array_get(network->nodes, event->node_id);
  payment = event->payment;
  next_hop = get_route_hop(event->node_id, payment->route->route_hops, 1);
  next_edge = array_get(network->edges, next_hop->edge_id);

  if(!is_present(next_edge->id, node->open_edges)) {
    printf("ERROR (forward_fail): edge %ld is not an edge of node %ld \n", next_edge->id, node->id);
    exit(-1);
  }

  next_hop->edges_lock_end_time = simulation->current_time;

  /* since the payment failed, the balance must be brought back to the state before the payment occurred */
  uint64_t prev_balance = next_edge->balance;
  next_edge->balance += next_hop->amount_to_forward;

  prev_hop = get_route_hop(event->node_id, payment->route->route_hops, 0);
  prev_node_id = prev_hop->from_node_id;
  event_type = prev_node_id == payment->sender ? RECEIVEFAIL : FORWARDFAIL;
  next_event_time = simulation->current_time + net_params.average_payment_forward_interval + (long)(fabs(net_params.variance_payment_forward_interval * gsl_ran_ugaussian(simulation->random_generator)));//prev_channel->latency;
  next_event = new_event(next_event_time, event_type, prev_node_id, event->payment);
  simulation->events = heap_insert(simulation->events, next_event, compare_event);
}

/* receive an HTLC fail (behavior of the payment sender node) */
void receive_fail(struct event* event, struct simulation* simulation, struct network* network, struct network_params net_params){
  struct payment* payment;
  struct route_hop* first_hop, *error_hop;
  struct edge* next_edge, *error_edge;
  struct event* next_event;
  struct node* node;
  uint64_t next_event_time;

  payment = event->payment;
  node = array_get(network->nodes, event->node_id);

  error_hop = payment->error.hop;
  error_edge = array_get(network->edges, error_hop->edge_id);
  if(error_hop->from_node_id != payment->sender){ // if the error occurred in the first hop, the balance hasn't to be updated, since it was not decreased
    first_hop = array_get(payment->route->route_hops, 0);
    next_edge = array_get(network->edges, first_hop->edge_id);
    if(!is_present(next_edge->id, node->open_edges)) {
      printf("ERROR (receive_fail): edge %ld is not an edge of node %ld \n", next_edge->id, node->id);
      exit(-1);
    }

    uint64_t prev_balance = next_edge->balance;
    next_edge->balance += first_hop->amount_to_forward;
  }

/* print FAIL_NO_BALANCE error
    struct channel* channel = array_get(network->channels, error_edge->channel_id);
    printf("\n\tERROR : RECEIVE_FAIL on sending payment(id=%ld, amount=%lu) at edge(id=%ld, balance=%lu, htlc_max_msat=%lu, channel_capacity=%lu) ", payment->id, payment->amount, error_edge->id, error_edge->balance, ((struct channel_update*)(error_edge->channel_updates->data))->htlc_maximum_msat, channel->capacity);
    printf("\n\tPATH  : ");
    for(int i = 0; i < array_len(payment->route->route_hops); i++){
        struct route_hop* hop = array_get(payment->route->route_hops, i);
        struct edge* edge = array_get(network->edges, hop->edge_id);
        printf("(edge_id=%ld,edge_balance=%lu,", edge->id, edge->balance);
        if(edge->group != NULL) {
            printf("group_id=%ld,group_cap=%lu)", edge->group->id, edge->group->group_cap);
        }else{
            printf("group_id=NULL,group_cap=NULL)");
        }
        if (i != array_len(payment->route->route_hops) - 1) printf("-");
    }
    printf("\n");
*/

    // record channel_update
    struct channel_update *channel_update = malloc(sizeof(struct channel_update));
    channel_update->htlc_maximum_msat = payment->amount;
    channel_update->edge_id = error_edge->id;
    channel_update->time = simulation->current_time;
    error_edge->channel_updates = push(error_edge->channel_updates, channel_update);

  add_attempt_history(payment, network, simulation->current_time, 0);

  next_event_time = simulation->current_time;
  next_event = new_event(next_event_time, FINDPATH, payment->sender, payment);
  simulation->events = heap_insert(simulation->events, next_event, compare_event);

    // channel update broadcast event
    struct event *channel_update_event = new_event(simulation->current_time + net_params.group_broadcast_delay, CHANNELUPDATEFAIL, node->id, payment);
    simulation->events = heap_insert(simulation->events, channel_update_event, compare_event);
}

// 送金に使用された全てのedgeのグループ更新を行う
struct element* request_group_update(struct event* event, struct simulation* simulation, struct network* network, struct network_params net_params, struct element* group_add_queue){

    for(long i = 0; i < array_len(event->payment->route->route_hops); i++){
        struct route_hop* hop = array_get(event->payment->route->route_hops, i);
        struct edge* edge = array_get(network->edges, hop->edge_id);
        struct edge* counter_edge = array_get(network->edges, edge->counter_edge_id);

        if(edge->group != NULL) {
            struct group* group = edge->group;
            int close_flg = update_group(edge->group, net_params, simulation->current_time, simulation->random_generator, net_params.enable_fake_balance_update, edge);

            if(close_flg){
                group->is_closed = simulation->current_time;

                // add edges to queue
                for(long j = 0; j < array_len(group->edges); j++){
                    struct edge* edge_in_group = array_get(group->edges, j);
                    edge_in_group->group = NULL;
                    group_add_queue = list_insert_sorted_position(group_add_queue, edge_in_group, (long (*)(void *)) get_edge_balance);
                }

                // construct_groups event
                uint64_t next_event_time = simulation->current_time;
                struct event* next_event = new_event(next_event_time, CONSTRUCTGROUPS, event->node_id, event->payment);
                simulation->events = heap_insert(simulation->events, next_event, compare_event);
            }
        }

        if(counter_edge->group != NULL) {
            struct group* group = counter_edge->group;
            int close_flg = update_group(counter_edge->group, net_params, simulation->current_time, simulation->random_generator, net_params.enable_fake_balance_update, counter_edge);

            if(close_flg){
                group->is_closed = simulation->current_time;

                // add edges to queue
                for(long j = 0; j < array_len(group->edges); j++){
                    struct edge* edge_in_group = array_get(group->edges, j);
                    edge_in_group->group = NULL;
                    group_add_queue = list_insert_sorted_position(group_add_queue, edge_in_group, (long (*)(void *)) get_edge_balance);
                }

                // construct_groups event
                uint64_t next_event_time = simulation->current_time;
                struct event* next_event = new_event(next_event_time, CONSTRUCTGROUPS, event->node_id, event->payment);
                simulation->events = heap_insert(simulation->events, next_event, compare_event);
            }
        }
    }

    return group_add_queue;
}

int can_join_group(struct group* group, struct edge* edge, enum routing_method routing_method){

    if(routing_method == GROUP_ROUTING){
        if(edge->balance < group->min_cap_limit || edge->balance > group->max_cap_limit){
            return 0;
        }

        for(int i = 0; i < array_len(group->edges); i++) {
            struct edge *e = array_get(group->edges, i);
            if (edge == e) return 0;
            if (edge->to_node_id == e->to_node_id ||
                edge->to_node_id == e->from_node_id ||
                edge->from_node_id == e->to_node_id ||
                edge->from_node_id == e->from_node_id) {
                return 0;
            }
        }

        return 1;
    }
    else if(routing_method == GROUP_ROUTING_CUL){

        if(group->group_cap < edge->balance - (uint64_t)((double)edge->balance * edge->policy.cul_threshold)) return 0;
        if(group->group_cap > edge->balance) return 0;

        for(int i = 0; i < array_len(group->edges); i++) {
            struct edge *e = array_get(group->edges, i);
            if (edge == e) return 0;
            if (edge->to_node_id == e->to_node_id ||
                edge->to_node_id == e->from_node_id ||
                edge->from_node_id == e->to_node_id ||
                edge->from_node_id == e->from_node_id) {
                return 0;
            }
        }

        return 1;
    }
    else{
        fprintf(stderr, "ERROR: can_join_group called with unsupported routing method %d\n", routing_method);
        exit(1);
    }
}

struct element* construct_groups(struct simulation* simulation, struct element* group_add_queue, struct network *network, struct network_params net_params){

    if(group_add_queue == NULL) return group_add_queue;

    for(struct element* iterator = group_add_queue; iterator != NULL; iterator = iterator->next){

        struct edge* requesting_edge = iterator->data;

        if(net_params.routing_method == GROUP_ROUTING) {

            // new group
            struct group* group = malloc(sizeof(struct group));
            group->edges = array_initialize(net_params.group_size);
            group->edges = array_insert(group->edges, requesting_edge);
            if(net_params.group_limit_rate != -1) {
                group->max_cap_limit = requesting_edge->balance + (uint64_t)((float)requesting_edge->balance * net_params.group_limit_rate);
                group->min_cap_limit = requesting_edge->balance - (uint64_t)((float)requesting_edge->balance * net_params.group_limit_rate);
                if(group->max_cap_limit < requesting_edge->balance) group->max_cap_limit = UINT64_MAX;
                if(group->min_cap_limit > requesting_edge->balance) group->min_cap_limit = 0;
            }else {
                group->max_cap_limit = UINT64_MAX;
                group->min_cap_limit = 0;
            }
            group->id = array_len(network->groups);
            group->is_closed = 0;
            group->constructed_time = simulation->current_time;
            group->history = NULL;

            // search the closest balance edge from neighbors
            struct element* bottom = iterator;
            struct element* top = iterator;
            while(bottom != NULL || top != NULL){

                // both edge are out of group limit, skip this group
                if(top != NULL && bottom != NULL){
                    struct edge* bottom_edge = bottom->data;
                    struct edge* top_edge = top->data;
                    if(bottom_edge->balance < group->min_cap_limit && top_edge->balance > group->max_cap_limit){
                        break;
                    }
                }

                // join bottom and top edge to group
                if(bottom != NULL){
                    struct edge* bottom_edge = bottom->data;
                    if(can_join_group(group, bottom_edge, net_params.routing_method)){
                        group->edges = array_insert(group->edges, bottom_edge);
                        if(array_len(group->edges) == net_params.group_size) break;
                    }
                    bottom = bottom->prev;
                }
                if(top != NULL){
                    struct edge* top_edge = top->data;
                    if(can_join_group(group, top_edge, net_params.routing_method)){
                        group->edges = array_insert(group->edges, top_edge);
                        if(array_len(group->edges) == net_params.group_size) break;
                    }
                    top = top->next;
                }
            }

            // register group
            if(array_len(group->edges) == net_params.group_size){
                // init group_cap
                update_group(group, net_params, simulation->current_time, simulation->random_generator, net_params.enable_fake_balance_update, NULL);
                network->groups = array_insert(network->groups, group);
                for(int i = 0; i < array_len(group->edges); i++){
                    struct edge* group_member_edge = array_get(group->edges, i);
                    group_add_queue = list_delete(group_add_queue, &iterator, group_member_edge, (int (*)(void *, void *)) is_equal_edge);
                    group_member_edge->group = group;
                }
                if(iterator == NULL) break;
            }else{
                array_free(group->edges);
                free(group);
            }
        }
        else if (net_params.routing_method == GROUP_ROUTING_CUL) {
            struct group* group = malloc(sizeof(struct group));
            group->edges = array_initialize(net_params.group_size);
            group->edges = array_insert(group->edges, requesting_edge);
            group->min_cap_limit = 0;
            group->max_cap_limit = 0;
            group->id = array_len(network->groups);
            group->is_closed = 0;
            group->constructed_time = simulation->current_time;
            group->history = NULL;

            // グループ構築次のみ、requesting_edgeの容量秘匿のため、一時的にgroup_capを以下の値に設定する
            group->group_cap = requesting_edge->balance - (uint64_t)((double)requesting_edge->balance * requesting_edge->policy.cul_threshold);

            // search the closest balance edge from neighbors
            struct element* i_bottom = iterator;
            struct element* i_top = iterator;
            while(i_bottom != NULL || i_top != NULL){

                // both edge are out of group limit, skip this group
                if(i_top != NULL && i_bottom != NULL){
                    struct edge* bottom_edge = i_bottom->data;
                    struct edge* top_edge = i_top->data;

                    // group_reqブロードキャストメッセージを受け取ったedgeが範囲外でグループに所属できない場合の判定
                    // https://www.notion.so/cul-230d4e598f3480d5882ef98559d5caaa?source=copy_link
                    if(group->group_cap > top_edge->balance) break;
                    if(group->group_cap < top_edge->balance - (uint64_t)((double)top_edge->balance * top_edge->policy.cul_threshold)) break;
                    if(group->group_cap > bottom_edge->balance) break;
                    if(group->group_cap < bottom_edge->balance - (uint64_t)((double)bottom_edge->balance * bottom_edge->policy.cul_threshold)) break;
                }

                // join i_bottom and i_top edge to group
                if(i_bottom != NULL){
                    struct edge* bottom_edge = i_bottom->data;
                    if(can_join_group(group, bottom_edge, net_params.routing_method)){
                        group->edges = array_insert(group->edges, bottom_edge);
                        if(array_len(group->edges) == net_params.group_size) break;
                    }
                    i_bottom = i_bottom->prev;
                }
                if(i_top != NULL){
                    struct edge* top_edge = i_top->data;
                    if(can_join_group(group, top_edge, net_params.routing_method)){
                        group->edges = array_insert(group->edges, top_edge);
                        if(array_len(group->edges) == net_params.group_size) break;
                    }
                    i_top = i_top->next;
                }
            }

            // register group
            if(array_len(group->edges) == net_params.group_size){
                // init group_cap
                update_group(group, net_params, simulation->current_time, simulation->random_generator, net_params.enable_fake_balance_update, NULL);
                network->groups = array_insert(network->groups, group);
                for(int i = 0; i < array_len(group->edges); i++){
                    struct edge* group_member_edge = array_get(group->edges, i);
                    group_add_queue = list_delete(group_add_queue, &iterator, group_member_edge, (int (*)(void *, void *)) is_equal_edge);
                    group_member_edge->group = group;
                }
                if(iterator == NULL) break;
            }else{
                array_free(group->edges);
                free(group);
            }
        }
    }
    return group_add_queue;
}

void channel_update_success(struct event* event, struct simulation* simulation, struct network* network){
    struct node* node = array_get(network->nodes, event->node_id);
    process_success_result(node, event->payment, simulation->current_time);
}

void channel_update_fail(struct event* event, struct simulation* simulation, struct network* network){
    struct node* node = array_get(network->nodes, event->node_id);
    process_fail_result(node, event->payment, simulation->current_time);
}