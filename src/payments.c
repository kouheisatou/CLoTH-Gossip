#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <math.h>
#include <inttypes.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_math.h>
#include "../include/array.h"
#include "../include/htlc.h"
#include "../include/payments.h"
#include "../include/network.h"

/* Functions in this file generate the payments that are exchanged in the payment-channel network during the simulation */


struct payment* new_payment(long id, long sender, long receiver, uint64_t amount, uint64_t start_time, uint64_t max_fee_limit) {
  struct payment * p;
  p = malloc(sizeof(struct payment));
  p->id=id;
  p->sender= sender;
  p->receiver = receiver;
  p->amount = amount;
  p->start_time = start_time;
  p->route = NULL;
  p->is_success = 0;
  p->offline_node_count = 0;
  p->no_balance_count = 0;
//  p->edge_occupied_count = 0;
  p->is_timeout = 0;
  p->end_time = 0;
  p->attempts = 0;
  p->error.type = NOERROR;
  p->error.hop = NULL;
  p->is_shard = 0;
  p->parent_payment_id = -1;
  p->child_shard_ids = NULL;
  p->split_depth = 0;
  p->pending_shards_count = 0;
  p->is_complete = 0;
  p->locked_route_hops = NULL;
  p->is_rolled_back = 0;
  p->history = NULL;
  p->max_fee_limit = max_fee_limit;
  return p;
}


/* generate random payments and store them in "payments.csv" */
void generate_random_payments(struct payments_params pay_params, long n_nodes, gsl_rng * random_generator) {
  long i, sender_id, receiver_id;
  uint64_t  payment_amount=0, payment_time=1, next_payment_interval, max_fee_limit=UINT64_MAX;
  long payment_idIndex=0;
  FILE* payments_file;

  payments_file = fopen("payments.csv", "w");
  if(payments_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file payments.csv\n");
    exit(-1);
  }
  fprintf(payments_file, "id,sender_id,receiver_id,amount,start_time,max_fee_limit\n");

  for(i=0;i<pay_params.n_payments;i++) {
    do{
      sender_id = gsl_rng_uniform_int(random_generator,n_nodes);
      receiver_id = gsl_rng_uniform_int(random_generator, n_nodes);
    } while(sender_id==receiver_id);
    payment_amount = fabs(pay_params.amount_mu + gsl_ran_ugaussian(random_generator) * pay_params.amount_sigma)*1000.0; // convert satoshi to millisatoshi
    /* payment interarrival time is an exponential (Poisson process) whose mean is the inverse of payment rate
       (expressed in payments per second, then multiplied to convert in milliseconds)
     */
    next_payment_interval = 1000*gsl_ran_exponential(random_generator, pay_params.inverse_payment_rate);
    payment_time += next_payment_interval;
    if(pay_params.max_fee_limit_sigma != -1 && pay_params.max_fee_limit_mu != -1) {
        max_fee_limit = fabs(pay_params.max_fee_limit_mu + gsl_ran_ugaussian(random_generator) * pay_params.max_fee_limit_sigma)*1000.0; // convert satoshi to millisatoshi
    }
    fprintf(payments_file, "%ld,%ld,%ld,%ld,%ld,%ld\n", payment_idIndex++, sender_id, receiver_id, payment_amount, payment_time, max_fee_limit);
  }

  fclose(payments_file);
}

/* generate payments from file */
struct array* generate_payments(struct payments_params pay_params) {
  struct payment* payment;
  char row[256], payments_filename[256];
  long id, sender, receiver;
  uint64_t amount, time, max_fee_limit;
  struct array* payments;
  FILE* payments_file;

  if(!(pay_params.payments_from_file))
    strcpy(payments_filename, "payments.csv");
  else
    strcpy(payments_filename, pay_params.payments_filename);

  payments_file = fopen(payments_filename, "r");
  if(payments_file==NULL) {
    printf("ERROR: cannot open file <%s>\n", payments_filename);
    exit(-1);
  }

  payments = array_initialize(1000);

  fgets(row, 256, payments_file);
  while(fgets(row, 256, payments_file) != NULL) {
    sscanf(row, "%ld,%ld,%ld,%"SCNu64",%"SCNu64",%"SCNu64"", &id, &sender, &receiver, &amount, &time, &max_fee_limit);
    payment = new_payment(id, sender, receiver, amount, time, max_fee_limit);
    payments = array_insert(payments, payment);
  }
  fclose(payments_file);

  return payments;
}


struct array* initialize_payments(struct payments_params pay_params, long n_nodes, gsl_rng* random_generator) {
  if(!(pay_params.payments_from_file))
    generate_random_payments(pay_params, n_nodes, random_generator);
  return generate_payments(pay_params);
}

void add_attempt_history(struct payment* pmt, struct network* network, uint64_t time, short is_succeeded){
  struct attempt* attempt = malloc(sizeof(struct attempt));
  attempt->attempts = pmt->attempts;
  attempt->end_time = time;
  if(is_succeeded){
    attempt->error_edge_id = 0;
    attempt->error_type = NOERROR;
  }else{
    attempt->error_edge_id = pmt->error.hop->edge_id;
    attempt->error_type = pmt->error.type;
  }
  attempt->is_succeeded = is_succeeded;

  // Initialize MPP split information
  attempt->split_occurred = 0;
  attempt->child_shard_id1 = -1;
  attempt->child_shard_id2 = -1;
  attempt->child_shard_amount1 = 0;
  attempt->child_shard_amount2 = 0;
  attempt->split_depth = pmt->split_depth;
  strcpy(attempt->split_reason, "");

  long route_len = array_len(pmt->route->route_hops);
  attempt->route = array_initialize(route_len);

  for(int i = 0; i < route_len; i++){
    struct route_hop* route_hop = array_get(pmt->route->route_hops, i);
    struct edge* edge = array_get(network->edges, route_hop->edge_id);
    short is_in_group = 0;
    if(edge->group != NULL) is_in_group = 1;
    attempt->route = array_insert(attempt->route, take_edge_snapshot(edge, route_hop->amount_to_forward, is_in_group, route_hop->group_cap));
  }

  pmt->history = push(pmt->history, attempt);
}

void add_split_history(struct payment* pmt, uint64_t time, long child1_id, long child2_id, uint64_t child1_amount, uint64_t child2_amount, const char* reason){
  struct attempt* attempt = malloc(sizeof(struct attempt));
  attempt->attempts = pmt->attempts;
  attempt->end_time = time;
  attempt->error_edge_id = 0;
  attempt->error_type = NOERROR;
  attempt->is_succeeded = 0; // Split means current payment didn't succeed as-is
  attempt->route = NULL; // No route for split events

  // Record MPP split information
  attempt->split_occurred = 1;
  attempt->child_shard_id1 = child1_id;
  attempt->child_shard_id2 = child2_id;
  attempt->child_shard_amount1 = child1_amount;
  attempt->child_shard_amount2 = child2_amount;
  attempt->split_depth = pmt->split_depth;
  strncpy(attempt->split_reason, reason, 255);
  attempt->split_reason[255] = '\0';

  pmt->history = push(pmt->history, attempt);
}

void add_failure_history(struct payment* pmt, uint64_t time, const char* reason){
  struct attempt* attempt = malloc(sizeof(struct attempt));
  attempt->attempts = pmt->attempts;
  attempt->end_time = time;
  attempt->error_edge_id = 0;
  attempt->error_type = NOERROR;
  attempt->is_succeeded = 0;
  attempt->route = NULL;

  // Record failure information
  attempt->split_occurred = 0;
  attempt->child_shard_id1 = -1;
  attempt->child_shard_id2 = -1;
  attempt->child_shard_amount1 = 0;
  attempt->child_shard_amount2 = 0;
  attempt->split_depth = pmt->split_depth;
  strncpy(attempt->split_reason, reason, 255);
  attempt->split_reason[255] = '\0';

  pmt->history = push(pmt->history, attempt);
}