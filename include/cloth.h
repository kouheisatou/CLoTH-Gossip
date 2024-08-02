#ifndef CLOTH_H
#define CLOTH_H

#include <stdint.h>
#include "heap.h"
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

enum routing_method{
    CLOTH_ORIGINAL, CHANNEL_UPDATE, GROUP_ROUTING, IDEAL
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
  unsigned int group_cap_update;
  unsigned int group_broadcast_delay;
  int group_size;
  float group_limit_rate;
};

struct payments_params{
  double inverse_payment_rate;
  long n_payments;
  double average_amount;
  unsigned int payments_from_file;
  char payments_filename[256];
  unsigned int log_all_events;
  unsigned int mpp;
};

struct simulation{
  uint64_t current_time; //milliseconds
  struct heap* events;
  gsl_rng* random_generator;
};

#endif
