#ifndef CLOTH_H
#define CLOTH_H

#include <stdint.h>
#include "heap.h"
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

enum routing_method{
    LN, // Routing by Frequency of pmt_fail_msg like Lightning Network
    FAB, // Failed Amount Broadcast
    GCB_MIN, // Group Capacity Broadcast (Broadcast Minimum Capacity in the Group)
    GCB_AVE, // Group Capacity Broadcast (Broadcast Average Capacity of the Group)
    RBB // Real-time Balance Broadcast
};

struct network_params{
  long n_nodes;
  long n_channels;
  long capacity_per_channel;
  double faulty_node_prob;
  unsigned int network_from_file;
  char nodes_filename[256];
  char channels_filename[256];
  char edges_filename[256];
  unsigned int payment_timeout; // set -1 to disable payment timeout
  unsigned int average_payment_forward_interval;
  unsigned int variance_payment_forward_interval;
  enum routing_method routing_method;
  unsigned int group_broadcast_delay;
  int group_size;
  float group_limit_rate;
};

struct payments_params{
  double inverse_payment_rate;
  long n_payments;
  double amount_mu; // average_payment_amount [satoshi]
  double amount_sigma; // variance_payment_amount [satoshi]
  unsigned int payments_from_file;
  char payments_filename[256];
  unsigned int mpp;
  double max_fee_limit_mu; // average_max_fee_limit [satoshi]
  double max_fee_limit_sigma; // variance_max_fee_limit [satoshi]
};

struct simulation{
  uint64_t current_time; //milliseconds
  struct heap* events;
  gsl_rng* random_generator;
};

#endif
