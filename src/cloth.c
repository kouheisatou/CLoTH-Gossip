#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <inttypes.h>
#include <dirent.h>

#include <gsl/gsl_rng.h>
#include <gsl/gsl_cdf.h>

#include "../include/payments.h"
#include "../include/heap.h"
#include "../include/array.h"
#include "../include/routing.h"
#include "../include/htlc.h"
#include "../include/list.h"
#include "../include/cloth.h"
#include "../include/network.h"
#include "../include/event.h"

/* This file contains the main, where the simulation logic is executed;
   additionally, it contains the the initialization functions,
   a function that reads the input and a function that writes the output values in csv files */


/* write the final values of nodes, channels, edges and payments in csv files */
void write_output(struct network* network, struct array* payments, char output_dir_name[]) {
  FILE* csv_channel_output, *csv_group_output, *csv_edge_output, *csv_payment_output, *csv_node_output;
  long i,j, *id;
  struct channel* channel;
  struct edge* edge;
  struct payment* payment;
  struct node* node;
  struct route* route;
  struct array* hops;
  struct route_hop* hop;
  DIR* results_dir;
  char output_filename[512];

  results_dir = opendir(output_dir_name);
  if(!results_dir){
    printf("cloth.c: Cannot find the output directory. The output will be stored in the current directory.\n");
    strcpy(output_dir_name, "./");
  }

  strcpy(output_filename, output_dir_name);
  strcat(output_filename, "channels_output.csv");
  csv_channel_output = fopen(output_filename, "w");
  if(csv_channel_output  == NULL) {
    printf("ERROR cannot open channel_output.csv\n");
    exit(-1);
  }
  fprintf(csv_channel_output, "id,edge1,edge2,node1,node2,capacity,is_closed\n");
  for(i=0; i<array_len(network->channels); i++) {
    channel = array_get(network->channels, i);
    fprintf(csv_channel_output, "%ld,%ld,%ld,%ld,%ld,%ld,%d\n", channel->id, channel->edge1, channel->edge2, channel->node1, channel->node2, channel->capacity, channel->is_closed);
  }
  fclose(csv_channel_output);

  strcpy(output_filename, output_dir_name);
  strcat(output_filename, "groups_output.csv");
  csv_group_output = fopen(output_filename, "w");
  if(csv_group_output  == NULL) {
    printf("ERROR cannot open groups_output.csv\n");
    exit(-1);
  }
  fprintf(csv_group_output, "id,edges,is_closed(closed_time),constructed_time,min_cap_limit,max_cap_limit,group_update_history,cul_average,used_count\n");
  for(i=0; i<array_len(network->groups); i++) {
    struct group *group = array_get(network->groups, i);
    fprintf(csv_group_output, "%ld,", group->id);
    long n_members = array_len(group->edges);
    for(j=0; j< n_members; j++){
        struct edge* edge_snapshot = array_get(group->edges, j);
        fprintf(csv_group_output, "%ld", edge_snapshot->id);
        if(j < n_members -1){
            fprintf(csv_group_output, "-");
        }else{
            fprintf(csv_group_output, ",");
        }
    }
    fprintf(csv_group_output, "%lu,%lu,%lu,%lu,", group->is_closed, group->constructed_time, group->min_cap_limit, group->max_cap_limit);
    fprintf(csv_group_output, "\"[");
    float cul_avg = 0.0f;
    for(struct element* iterator = group->history; iterator != NULL; iterator = iterator->next) {
        struct group_update* group_update = iterator->data;
        fprintf(csv_group_output, "{\"\"edge_balances\"\":[");
        float sum_cul = 0.0f;
        for(j=0; j<n_members; j++) {
            struct edge* e = array_get(group->edges, j);
            float cul = (1.0f - ((float)group_update->group_cap / (float)group_update->edge_balances[j]));
            sum_cul += cul;
            if(group_update->fake_balance_updated_edge_id == e->id){
                fprintf(csv_group_output, "{\"\"edge_id\"\":%ld,\"\"balance\"\":%ld,\"\"cul\"\":%f,\"\"fake_balance_update\"\":%s,\"\"actual_balance\"\":%ld}", e->id, group_update->edge_balances[j], cul, "true", group_update->fake_balance_updated_edge_actual_balance);
            }else{
                fprintf(csv_group_output, "{\"\"edge_id\"\":%ld,\"\"balance\"\":%ld,\"\"cul\"\":%f,\"\"fake_balance_update\"\":%s}", e->id, group_update->edge_balances[j], cul, "false");
            }
            if(j < n_members - 1) {
                fprintf(csv_group_output, ",");
            }
        }
        float cul = sum_cul / (float)n_members;
        cul_avg += cul / (float) list_len(group->history);
        fprintf(csv_group_output, "],\"\"time\"\":%lu,\"\"group_cap\"\":%lu,\"\"cul_avg\"\":%f,\"\"triggered_edge_id\"\":%ld}", group_update->time, group_update->group_cap, cul, group_update->fake_balance_updated_edge_id, group_update->fake_balance_updated_edge_actual_balance, group_update->triggered_edge_id);
        if(iterator->next != NULL) {
            fprintf(csv_group_output, ",");
        }
    }
    fprintf(csv_group_output, "]\",%f,%ld\n", cul_avg, list_len(group->history)-1);
  }
  fclose(csv_group_output);

  strcpy(output_filename, output_dir_name);
  strcat(output_filename, "edges_output.csv");
  csv_edge_output = fopen(output_filename, "w");
  if(csv_edge_output  == NULL) {
    printf("ERROR cannot open edge_output.csv\n");
    exit(-1);
  }
  fprintf(csv_edge_output, "id,channel_id,counter_edge_id,from_node_id,to_node_id,balance,fee_base,fee_proportional,min_htlc,timelock,is_closed,tot_flows,cul_threshold,channel_updates,group\n");
  for(i=0; i<array_len(network->edges); i++) {
    edge = array_get(network->edges, i);
    fprintf(csv_edge_output, "%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%d,%d,%ld,%lf,", edge->id, edge->channel_id, edge->counter_edge_id, edge->from_node_id, edge->to_node_id, edge->balance, edge->policy.fee_base, edge->policy.fee_proportional, edge->policy.min_htlc, edge->policy.timelock, edge->is_closed, edge->tot_flows, edge->policy.cul_threshold);
    char channel_updates_text[1000000] = "";
    for (struct element *iterator = edge->channel_updates; iterator != NULL; iterator = iterator->next) {
        struct channel_update *channel_update = iterator->data;
        char temp[1000000];
        int written = 0;
        if(iterator->next != NULL) {
            written = snprintf(temp, sizeof(temp), "-%ld%s", channel_update->htlc_maximum_msat, channel_updates_text);
        }else{
            written = snprintf(temp, sizeof(temp), "%ld%s", channel_update->htlc_maximum_msat, channel_updates_text);
        }
        // Check if the output was truncated
        if (written < 0 || (size_t)written >= sizeof(temp)) {
            fprintf(stderr, "Error: Buffer overflow detected.\n");
            exit(1);
        }
        strncpy(channel_updates_text, temp, sizeof(channel_updates_text) - 1);
    }
    fprintf(csv_edge_output, "%s,", channel_updates_text);
    if(edge->group == NULL){
        fprintf(csv_edge_output, "NULL\n");
    }else{
        fprintf(csv_edge_output, "%ld\n", edge->group->id);
    }
  }
  fclose(csv_edge_output);

  strcpy(output_filename, output_dir_name);
  strcat(output_filename, "payments_output.csv");
  csv_payment_output = fopen(output_filename, "w");
  if(csv_payment_output  == NULL) {
    printf("ERROR cannot open payment_output.csv\n");
    exit(-1);
  }
  fprintf(csv_payment_output, "id,sender_id,receiver_id,amount,start_time,max_fee_limit,end_time,mpp,is_shard,parent_payment_id,shards,is_success,no_balance_count,offline_node_count,timeout_exp,attempts,route,total_fee,attempts_history\n");
  for(i=0; i<array_len(payments); i++)  {
    payment = array_get(payments, i);
    // Output all payments including shards
    fprintf(csv_payment_output, "%ld,%ld,%ld,%ld,%ld,%ld,%ld,%u,%u,%ld,", 
            payment->id, payment->sender, payment->receiver, payment->amount, 
            payment->start_time, payment->max_fee_limit, payment->end_time, 
            payment->mpp_triggered, payment->is_shard, payment->parent_id);
    // Output shards array
    if(payment->shards_id[0] != -1 && payment->shards_id[1] != -1) {
      fprintf(csv_payment_output, "%ld-%ld,", payment->shards_id[0], payment->shards_id[1]);
    } else {
      fprintf(csv_payment_output, ",");
    }
    fprintf(csv_payment_output, "%u,%d,%d,%u,%d,", 
            payment->is_success, payment->no_balance_count, payment->offline_node_count, 
            payment->is_timeout, payment->attempts);
    route = payment->route;
    if(route==NULL)
      fprintf(csv_payment_output, ",,");
    else {
      hops = route->route_hops;
      for(j=0; j<array_len(hops); j++) {
        hop = array_get(hops, j);
        if(j==array_len(hops)-1)
          fprintf(csv_payment_output,"%ld,",hop->edge_id);
        else
          fprintf(csv_payment_output,"%ld-",hop->edge_id);
      }
      fprintf(csv_payment_output, "%ld,",route->total_fee);
    }
    // build attempts history json
    if(payment->history != NULL) {
        fprintf(csv_payment_output, "\"[");
        for (struct element *iterator = payment->history; iterator != NULL; iterator = iterator->next) {
            struct attempt *attempt = iterator->data;
            if(attempt->is_split) {
                // Split event
                fprintf(csv_payment_output, "{\"\"attempts\"\":%d,\"\"is_split\"\":true,\"\"end_time\"\":%llu,\"\"shard1_id\"\":%ld,\"\"shard2_id\"\":%ld}", 
                        attempt->attempts, attempt->end_time, attempt->shard1_id, attempt->shard2_id);
            } else {
                // Normal attempt (success or failure)
                fprintf(csv_payment_output, "{\"\"attempts\"\":%d,\"\"is_succeeded\"\":%d,\"\"end_time\"\":%llu,\"\"error_edge\"\":%ld,\"\"error_type\"\":%d,\"\"route\"\":[", 
                        attempt->attempts, attempt->is_succeeded, attempt->end_time, attempt->error_edge_id, attempt->error_type);
                if(attempt->route != NULL) {
                    for (j = 0; j < array_len(attempt->route); j++) {
                        struct edge_snapshot* edge_snapshot = array_get(attempt->route, j);
                        edge = array_get(network->edges, edge_snapshot->id);
                        channel = array_get(network->channels, edge->channel_id);
                        fprintf(csv_payment_output,"{\"\"edge_id\"\":%ld,\"\"from_node_id\"\":%ld,\"\"to_node_id\"\":%ld,\"\"sent_amt\"\":%llu,\"\"edge_cap\"\":%llu,\"\"channel_cap\"\":%llu,", 
                                edge_snapshot->id, edge->from_node_id, edge->to_node_id, edge_snapshot->sent_amt, edge_snapshot->balance, channel->capacity);
                        if(edge_snapshot->is_in_group) fprintf(csv_payment_output, "\"\"group_cap\"\":%llu,", edge_snapshot->group_cap);
                        else fprintf(csv_payment_output,"\"\"group_cap\"\":null,");
                        if(edge_snapshot->does_channel_update_exist) fprintf(csv_payment_output,"\"\"channel_update\"\":%llu}", edge_snapshot->last_channle_update_value);
                        else fprintf(csv_payment_output,"\"\"channel_update\"\":null}");
                        if (j != array_len(attempt->route) - 1) fprintf(csv_payment_output, ",");
                    }
                }
                fprintf(csv_payment_output, "]}");
            }
            if (iterator->next != NULL) fprintf(csv_payment_output, ",");
            else fprintf(csv_payment_output, "]");
        }
        fprintf(csv_payment_output, "\"");
    }
    fprintf(csv_payment_output, "\n");
  }
  fclose(csv_payment_output);

  strcpy(output_filename, output_dir_name);
  strcat(output_filename, "nodes_output.csv");
  csv_node_output = fopen(output_filename, "w");
  if(csv_node_output  == NULL) {
    printf("ERROR cannot open nodes_output.csv\n");
    return;
  }
  fprintf(csv_node_output, "id,open_edges\n");
  for(i=0; i<array_len(network->nodes); i++) {
    node = array_get(network->nodes, i);
    fprintf(csv_node_output, "%ld,", node->id);
    if(array_len(node->open_edges)==0)
      fprintf(csv_node_output, "-1");
    else {
      for(j=0; j<array_len(node->open_edges); j++) {
        id = array_get(node->open_edges, j);
        if(j==array_len(node->open_edges)-1)
          fprintf(csv_node_output,"%ld",*id);
        else
          fprintf(csv_node_output,"%ld-",*id);
      }
    }
    fprintf(csv_node_output,"\n");
  }
  fclose(csv_node_output);
}


void initialize_input_parameters(struct network_params *net_params, struct payments_params *pay_params) {
  net_params->n_nodes = net_params->n_channels = net_params->capacity_per_channel = 0;
  net_params->faulty_node_prob = 0.0;
  net_params->network_from_file = 0;
  strcpy(net_params->nodes_filename, "\0");
  strcpy(net_params->channels_filename, "\0");
  strcpy(net_params->edges_filename, "\0");
  pay_params->inverse_payment_rate = pay_params->amount_mu = 0.0;
  pay_params->n_payments = 0;
  pay_params->payments_from_file = 0;
  strcpy(pay_params->payments_filename, "\0");
  pay_params->mpp = 0;
  pay_params->max_shard_count = 16; // default max shard count
}


/* parse the input parameters in "cloth_input.txt" */
void read_input(struct network_params* net_params, struct payments_params* pay_params){
  FILE* input_file;
  char *parameter, *value, line[1024];

  initialize_input_parameters(net_params, pay_params);

  input_file = fopen("cloth_input.txt","r");

  if(input_file==NULL){
    fprintf(stderr, "ERROR: cannot open file <cloth_input.txt> in current directory.\n");
    exit(-1);
  }

  while(fgets(line, 1024, input_file)){

    parameter = strtok(line, "=");
    value = strtok(NULL, "=");
    if(parameter==NULL || value==NULL){
      fprintf(stderr, "ERROR: wrong format in file <cloth_input.txt>\n");
      fclose(input_file);
      exit(-1);
    }

    if(value[0]==' ' || parameter[strlen(parameter)-1]==' '){
      fprintf(stderr, "ERROR: no space allowed after/before <=> character in <cloth_input.txt>. Space detected in parameter <%s>\n", parameter);
      fclose(input_file);
      exit(-1);
    }

    value[strlen(value)-1] = '\0';

    if(strcmp(parameter, "generate_network_from_file")==0){
      if(strcmp(value, "true")==0)
        net_params->network_from_file=1;
      else if(strcmp(value, "false")==0)
        net_params->network_from_file=0;
      else{
        fprintf(stderr, "ERROR: wrong value of parameter <%s> in <cloth_input.txt>. Possible values are <true> or <false>\n", parameter);
        fclose(input_file);
        exit(-1);
      }
    }
    else if(strcmp(parameter, "nodes_filename")==0){
      strcpy(net_params->nodes_filename, value);
    }
    else if(strcmp(parameter, "channels_filename")==0){
      strcpy(net_params->channels_filename, value);
    }
    else if(strcmp(parameter, "edges_filename")==0){
      strcpy(net_params->edges_filename, value);
    }
    else if(strcmp(parameter, "n_additional_nodes")==0){
      net_params->n_nodes = strtol(value, NULL, 10);
    }
    else if(strcmp(parameter, "n_channels_per_node")==0){
      net_params->n_channels = strtol(value, NULL, 10);
    }
    else if(strcmp(parameter, "capacity_per_channel")==0){
      net_params->capacity_per_channel = strtol(value, NULL, 10);
    }
    else if(strcmp(parameter, "faulty_node_probability")==0){
      net_params->faulty_node_prob = strtod(value, NULL);
    }
    else if(strcmp(parameter, "generate_payments_from_file")==0){
      if(strcmp(value, "true")==0)
        pay_params->payments_from_file=1;
      else if(strcmp(value, "false")==0)
        pay_params->payments_from_file=0;
      else{
        fprintf(stderr, "ERROR: wrong value of parameter <%s> in <cloth_input.txt>. Possible values are <true> or <false>\n", parameter);
        fclose(input_file);
        exit(-1);
      }
    }
    else if(strcmp(parameter, "enable_fake_balance_update")==0){
      if(strcmp(value, "true")==0)
        net_params->enable_fake_balance_update = 1;
      else if(strcmp(value, "false")==0)
        net_params->enable_fake_balance_update = 0;
      else{
        fprintf(stderr, "ERROR: wrong value of parameter <%s> in <cloth_input.txt>. Possible values are <true> or <false>\n", parameter);
        fclose(input_file);
        exit(-1);
      }
    }
    else if(strcmp(parameter, "payment_timeout")==0) {
        net_params->payment_timeout=strtol(value, NULL, 10);
    }
    else if(strcmp(parameter, "average_payment_forward_interval")==0) {
        net_params->average_payment_forward_interval=strtol(value, NULL, 10);
    }
    else if(strcmp(parameter, "variance_payment_forward_interval")==0) {
        net_params->variance_payment_forward_interval=strtol(value, NULL, 10);
    }
    else if(strcmp(parameter, "group_broadcast_delay")==0) {
        net_params->group_broadcast_delay=strtol(value, NULL, 10);
    }
    else if(strcmp(parameter, "routing_method")==0){
      if(strcmp(value, "cloth_original")==0)
        net_params->routing_method=CLOTH_ORIGINAL;
      else if(strcmp(value, "channel_update")==0)
        net_params->routing_method=CHANNEL_UPDATE;
      else if(strcmp(value, "group_routing_cul")==0)
        net_params->routing_method=GROUP_ROUTING_CUL;
      else if(strcmp(value, "group_routing")==0)
        net_params->routing_method=GROUP_ROUTING;
      else if(strcmp(value, "ideal")==0)
        net_params->routing_method=IDEAL;
      else{
        fprintf(stderr, "ERROR: wrong value of parameter <%s> in <cloth_input.txt>. Possible values are [\"cloth_original\", \"channel_update\", \"group_routing\", \"ideal\"]\n", parameter);
        fclose(input_file);
        exit(-1);
      }
    }
    else if(strcmp(parameter, "group_cap_update")==0){
      if(strcmp(value, "true")==0)
        net_params->group_cap_update=1;
      else if(strcmp(value, "false")==0)
        net_params->group_cap_update=0;
      else
        net_params->group_cap_update=-1;
    }
    else if(strcmp(parameter, "group_size")==0){
        if(strcmp(value, "")==0) net_params->group_size = -1;
        else net_params->group_size = strtol(value, NULL, 10);
    }
    else if(strcmp(parameter, "group_limit_rate")==0){
        if(strcmp(value, "")==0) net_params->group_limit_rate = -1;
        else net_params->group_limit_rate = strtof(value, NULL);
    }
    else if(strcmp(parameter, "cul_threshold_dist_alpha")==0){
        if(strcmp(value, "")==0) net_params->cul_threshold_dist_alpha = -1;
        else net_params->cul_threshold_dist_alpha = strtof(value, NULL);
    }
    else if(strcmp(parameter, "cul_threshold_dist_beta")==0){
        if(strcmp(value, "")==0) net_params->cul_threshold_dist_beta = -1;
        else net_params->cul_threshold_dist_beta = strtof(value, NULL);
    }
    else if(strcmp(parameter, "payments_filename")==0){
      strcpy(pay_params->payments_filename, value);
    }
    else if(strcmp(parameter, "payment_rate")==0){
      pay_params->inverse_payment_rate = 1.0/strtod(value, NULL);
    }
    else if(strcmp(parameter, "n_payments")==0){
      pay_params->n_payments = strtol(value, NULL, 10);
    }
    else if(strcmp(parameter, "average_payment_amount")==0){
      pay_params->amount_mu = strtod(value, NULL);
    }
    else if(strcmp(parameter, "variance_payment_amount")==0){
      pay_params->amount_sigma = strtod(value, NULL);
    }
    else if(strcmp(parameter, "mpp")==0){
      pay_params->mpp = strtoul(value, NULL, 10);
    }
    else if(strcmp(parameter, "average_max_fee_limit")==0){
        pay_params->max_fee_limit_mu = strtod(value, NULL);
    }
    else if(strcmp(parameter, "variance_max_fee_limit")==0){
        pay_params->max_fee_limit_sigma = strtod(value, NULL);
    }
    else if(strcmp(parameter, "max_shard_count")==0){
        pay_params->max_shard_count = strtol(value, NULL, 10);
    }
    else{
      fprintf(stderr, "ERROR: unknown parameter <%s>\n", parameter);
      fclose(input_file);
      exit(-1);
    }
  }
  // check invalid group settings
  if(net_params->routing_method == GROUP_ROUTING){
      if(net_params->group_limit_rate < 0 || net_params->group_limit_rate > 1){
          fprintf(stderr, "ERROR: wrong value of parameter <group_limit_rate> in <cloth_input.txt>.\n");
          exit(-1);
      }
      if(net_params->group_size < 0){
          fprintf(stderr, "ERROR: wrong value of parameter <group_size> in <cloth_input.txt>.\n");
          exit(-1);
      }
      if(net_params->group_cap_update == -1){
          fprintf(stderr, "ERROR: wrong value of parameter <group_cap_update> in <cloth_input.txt>.\n");
          exit(-1);
      }
  }
  fclose(input_file);
}


unsigned int has_shards(struct payment* payment){
  return (payment->shards_id[0] != -1 && payment->shards_id[1] != -1);
}


/* Recursively collect stats from a shard and its children */
static void collect_shard_stats(struct array* payments, struct payment* shard, 
                                uint64_t* max_end_time, int* all_success, 
                                int* no_balance_count, int* offline_node_count,
                                int* any_timeout, int* total_attempts, uint64_t* total_fee) {
  if(shard == NULL) return;
  
  // If this shard has children, process them recursively
  if(has_shards(shard)) {
    struct payment* child1 = array_get(payments, shard->shards_id[0]);
    struct payment* child2 = array_get(payments, shard->shards_id[1]);
    collect_shard_stats(payments, child1, max_end_time, all_success, no_balance_count, 
                        offline_node_count, any_timeout, total_attempts, total_fee);
    collect_shard_stats(payments, child2, max_end_time, all_success, no_balance_count,
                        offline_node_count, any_timeout, total_attempts, total_fee);
  } else {
    // Leaf shard - collect stats
    if(shard->end_time > *max_end_time) *max_end_time = shard->end_time;
    if(!shard->is_success) *all_success = 0;
    *no_balance_count += shard->no_balance_count;
    *offline_node_count += shard->offline_node_count;
    if(shard->is_timeout) *any_timeout = 1;
    *total_attempts += shard->attempts;
    if(shard->route != NULL) *total_fee += shard->route->total_fee;
  }
}

/* process stats of payments that were split (mpp payments) */
void post_process_payment_stats(struct array* payments){
  long i;
  struct payment* payment, *shard1, *shard2;
  for(i = 0; i < array_len(payments); i++){
    payment = array_get(payments, i);
    if(!has_shards(payment)) continue;
    
    // For root payments with shards, collect stats from all descendants
    if(payment->parent_id == -1 || payment->root_payment_id == payment->id) {
      uint64_t max_end_time = 0;
      int all_success = 1;
      int no_balance_count = 0;
      int offline_node_count = 0;
      int any_timeout = 0;
      int total_attempts = 0;
      uint64_t total_fee = 0;
      
      shard1 = array_get(payments, payment->shards_id[0]);
      shard2 = array_get(payments, payment->shards_id[1]);
      
      collect_shard_stats(payments, shard1, &max_end_time, &all_success, &no_balance_count,
                          &offline_node_count, &any_timeout, &total_attempts, &total_fee);
      collect_shard_stats(payments, shard2, &max_end_time, &all_success, &no_balance_count,
                          &offline_node_count, &any_timeout, &total_attempts, &total_fee);
      
      payment->end_time = max_end_time;
      payment->is_success = all_success;
      payment->no_balance_count = no_balance_count;
      payment->offline_node_count = offline_node_count;
      payment->is_timeout = any_timeout;
      payment->attempts = total_attempts;
      
      // Note: We don't set a route on the parent for MPP payments
      // Each shard has its own route in the output
    }
  }
}


gsl_rng* initialize_random_generator(){
  gsl_rng_env_setup();
  return gsl_rng_alloc (gsl_rng_default);
}


int main(int argc, char *argv[]) {
  struct event* event;
  clock_t  begin, end;
  double time_spent=0.0;
  long time_spent_thread = 0;
  struct network_params net_params;
  struct payments_params pay_params;
  struct timespec start, finish;
  struct network *network;
  long n_nodes, n_edges;
  struct array* payments;
  struct simulation* simulation;
  char output_dir_name[256];

  if(argc != 2) {
    fprintf(stderr, "ERROR cloth.c: please specify the output directory\n");
    return -1;
  }
  strcpy(output_dir_name, argv[1]);

  read_input(&net_params, &pay_params);

  simulation = malloc(sizeof(struct simulation));
  simulation->current_time = 0;

  simulation->random_generator = initialize_random_generator();
  printf("NETWORK INITIALIZATION\n");
  network = initialize_network(net_params, simulation->random_generator);
  n_nodes = array_len(network->nodes);
  n_edges = array_len(network->edges);

    // add edge which is not a member of any group to group_add_queue
    struct element* group_add_queue = NULL;
    if(net_params.routing_method == GROUP_ROUTING || net_params.routing_method == GROUP_ROUTING_CUL) {
        for (int i = 0; i < n_edges; i++) {
            group_add_queue = list_insert_sorted_position(group_add_queue, array_get(network->edges, i), (long (*)(void *)) get_edge_balance);
        }
        group_add_queue = construct_groups(simulation, group_add_queue, network, net_params);
    }
    printf("group_cover_rate on init : %f\n", (float)(array_len(network->edges) - list_len(group_add_queue)) / (float)(array_len(network->edges)));

  printf("PAYMENTS INITIALIZATION\n");
  payments = initialize_payments(pay_params,  n_nodes, simulation->random_generator);

  printf("EVENTS INITIALIZATION\n");
  simulation->events = initialize_events(payments);
  initialize_dijkstra(n_nodes, n_edges, payments);

  printf("INITIAL DIJKSTRA THREADS EXECUTION\n");
  clock_gettime(CLOCK_MONOTONIC, &start);
  run_dijkstra_threads(network, payments, 0, net_params.routing_method);
  clock_gettime(CLOCK_MONOTONIC, &finish);
  time_spent_thread = finish.tv_sec - start.tv_sec;
  printf("Time consumed by initial dijkstra executions: %ld s\n", time_spent_thread);

  printf("EXECUTION OF THE SIMULATION\n");

  /* core of the discrete-event simulation: extract next event, advance simulation time, execute the event */
  begin = clock();
  simulation->current_time = 1;
  long completed_payments = 0;
  while(heap_len(simulation->events) != 0) {
    event = heap_pop(simulation->events, compare_event);

    simulation->current_time = event->time;
    switch(event->type){
    case FINDPATH:
      find_path(event, simulation, network, &payments, pay_params, net_params);
      break;
    case SENDPAYMENT:
      send_payment(event, simulation, network, net_params);
      break;
    case FORWARDPAYMENT:
      forward_payment(event, simulation, network, net_params);
      break;
    case RECEIVEPAYMENT:
      receive_payment(event, simulation, network, net_params);
      break;
    case FORWARDSUCCESS:
      forward_success(event, simulation, network, net_params);
      break;
    case RECEIVESUCCESS:
      receive_success(event, simulation, network, net_params);
      break;
    case FORWARDFAIL:
      forward_fail(event, simulation, network, net_params);
      break;
    case RECEIVEFAIL:
      receive_fail(event, simulation, network, net_params);
      break;
    case OPENCHANNEL:
      open_channel(network, simulation->random_generator, net_params);
      break;
    case CHANNELUPDATEFAIL:
      channel_update_fail(event, simulation, network);
    case CHANNELUPDATESUCCESS:
      channel_update_success(event, simulation, network);
    case UPDATEGROUP:
      group_add_queue = request_group_update(event, simulation, network, net_params, group_add_queue);
      break;
    case CONSTRUCTGROUPS:
      group_add_queue = construct_groups(simulation, group_add_queue, network, net_params);
      break;
    default:
      printf("ERROR wrong event type\n");
      exit(-1);
    }

    struct payment* p = array_get(payments, event->payment->id);
    if(p->end_time != 0 && event->type != UPDATEGROUP && event->type != CONSTRUCTGROUPS && event->type != CHANNELUPDATEFAIL && event->type != CHANNELUPDATESUCCESS){
        completed_payments++;
        char progress_filename[512];
        strcpy(progress_filename, output_dir_name);
        strcat(progress_filename, "progress.tmp");
        FILE* progress_file = fopen(progress_filename, "w");
        if(progress_file != NULL){
            fprintf(progress_file, "%f", (float)completed_payments / (float)array_len(payments));
        }
        fclose(progress_file);
    }

    free(event);
  }
  printf("\n");
  end = clock();

  if(pay_params.mpp) {
    post_process_payment_stats(payments);
    
    // MPP summary statistics (only count root payments, not shards)
    int mpp_payments = 0, mpp_success = 0, total_shards = 0;
    for(long i = 0; i < array_len(payments); i++) {
      struct payment* p = array_get(payments, i);
      // Only count root payments that triggered MPP
      if(p->parent_id == -1 && (p->mpp_triggered || p->shard_count > 0)) {
        mpp_payments++;
        if(p->is_success) mpp_success++;
        total_shards += p->shard_count;
      }
    }
    printf("[MPP DEBUG] SUMMARY: mpp_payments=%d, mpp_success=%d, total_shards_created=%d\n",
           mpp_payments, mpp_success, total_shards);
  }

  time_spent = (double) (end - begin)/CLOCKS_PER_SEC;
  printf("Time consumed by simulation events: %lf s\n", time_spent);

  write_output(network, payments, output_dir_name);

    list_free(group_add_queue);
    free(simulation->random_generator);
    heap_free(simulation->events);
  free(simulation);

//    free_network(network);

  return 0;
}
