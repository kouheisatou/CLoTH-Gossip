#ifndef HTLC_H
#define HTLC_H

#include <stdint.h>
#include "array.h"
#include "routing.h"
#include "cloth.h"
#include "network.h"
#include "payments.h"
#include "event.h"

#define OFFLINELATENCY 3000 //3 seconds waiting for a node not responding (tcp default retransmission time)

/* a node pair result registers the most recent result of a payment (fail or success, with the corresponding amount and time)
   that occurred when the payment traversed an edge connecting the two nodes of the node pair */
struct node_pair_result{
  long to_node_id;
  uint64_t fail_time;
  uint64_t fail_amount;
  uint64_t success_time;
  uint64_t success_amount;
};


uint64_t compute_fee(uint64_t amount_to_forward, struct policy policy);

void find_path(struct event* event, struct simulation* simulation, struct network* network, struct array** payments, unsigned int mpp, enum routing_method routing_method);

struct element* send_payment(struct event* event, struct simulation* simulation, struct network* network, struct element* group_add_queue, struct network_params net_params, FILE* csv_group_update);

struct element* forward_payment(struct event* event, struct simulation* simulation, struct network* network, struct element* group_add_queue, struct network_params net_params, FILE* csv_group_update);

struct element* receive_payment(struct event* event, struct simulation* simulation, struct network* network, struct element* group_add_queue, struct network_params net_params, FILE* csv_group_update);

struct element* forward_success(struct event* event, struct simulation* simulation, struct network* network, struct element* group_add_queue, struct network_params net_params, FILE* csv_group_update);

void receive_success(struct event* event, struct simulation* simulation, struct network* network);

struct element* forward_fail(struct event* event, struct simulation* simulation, struct network* network, struct element* group_add_queue, struct network_params net_params, FILE* csv_group_update);

struct element* receive_fail(struct event* event, struct simulation* simulation, struct network* network, struct element* group_add_queue, struct network_params net_params, FILE* csv_group_update, FILE* csv_channel_update);

#endif
