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
#include "../include/hash.h"

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
  fprintf(csv_group_output, "id,edges,balances,is_closed(closed_time),constructed_time,min_cap_limit,max_cap_limit,max_edge_balance,min_edge_balance,group_capacity,cul\n");
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
    for(j=0; j< n_members; j++){
        struct edge* edge_snapshot = array_get(group->edges, j);
        fprintf(csv_group_output, "%lu", edge_snapshot->balance);
        if(j < n_members -1){
            fprintf(csv_group_output, "-");
        }else{
            fprintf(csv_group_output, ",");
        }
    }
    struct group_update* group_update;
    if(group->is_closed){
        group_update = group->history->next->data;
    }else{
        group_update = group->history->data;
    }
    float sum_cul = 0.0f;
    for(j=0; j< n_members; j++){
        sum_cul += (1.0f - ((float)group_update->group_cap / (float)group_update->edge_balances[j]));
    }
    fprintf(csv_group_output, "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%f\n", group->is_closed, group->constructed_time, group->min_cap_limit, group->max_cap_limit, group->max_cap, group->min_cap, group->group_cap, sum_cul / (float)n_members);
  }
  fclose(csv_group_output);

  strcpy(output_filename, output_dir_name);
  strcat(output_filename, "edges_output.csv");
  csv_edge_output = fopen(output_filename, "w");
  if(csv_edge_output  == NULL) {
    printf("ERROR cannot open edge_output.csv\n");
    exit(-1);
  }
  fprintf(csv_edge_output, "id,channel_id,counter_edge_id,from_node_id,to_node_id,balance,fee_base,fee_proportional,min_htlc,timelock,is_closed,tot_flows,channel_updates,group,locked_balance_and_duration\n");
  for(i=0; i<array_len(network->edges); i++) {
    edge = array_get(network->edges, i);
    fprintf(csv_edge_output, "%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%d,%d,%ld,", edge->id, edge->channel_id, edge->counter_edge_id, edge->from_node_id, edge->to_node_id, edge->balance, edge->policy.fee_base, edge->policy.fee_proportional, edge->policy.min_htlc, edge->policy.timelock, edge->is_closed, edge->tot_flows);
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
        fprintf(csv_edge_output, "NULL,");
    }else{
        fprintf(csv_edge_output, "%ld,", edge->group->id);
    }
    for(struct element* iterator = edge->edge_locked_balance_and_durations; iterator != NULL; iterator = iterator->next){
        struct edge_locked_balance_and_duration* edge_locked_balance_time = iterator->data;
        uint64_t locked_time = edge_locked_balance_time->locked_end_time - edge_locked_balance_time->locked_start_time;
        fprintf(csv_edge_output, "%lux%lu", edge_locked_balance_time->locked_balance, locked_time);
        if(iterator->next != NULL){
            fprintf(csv_edge_output, "-");
        }
    }
    fprintf(csv_edge_output, "\n");
  }
  fclose(csv_edge_output);

  strcpy(output_filename, output_dir_name);
  strcat(output_filename, "payments_output.csv");
  csv_payment_output = fopen(output_filename, "w");
  if(csv_payment_output  == NULL) {
    printf("ERROR cannot open payment_output.csv\n");
    exit(-1);
  }
  fprintf(csv_payment_output, "id,sender_id,receiver_id,amount,start_time,end_time,mpp,is_success,no_balance_count,offline_node_count,timeout_exp,attempts,route,total_fee,attempts_history\n");
  for(i=0; i<array_len(payments); i++)  {
    payment = array_get(payments, i);
    if (payment->id == -1) continue;
    fprintf(csv_payment_output, "%ld,%ld,%ld,%ld,%ld,%ld,%u,%u,%d,%d,%u,%d,", payment->id, payment->sender, payment->receiver, payment->amount, payment->start_time, payment->end_time, payment->is_shard, payment->is_success, payment->no_balance_count, payment->offline_node_count, payment->is_timeout, payment->attempts);
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
            fprintf(csv_payment_output, "{\"\"attempts\"\":%d,\"\"is_succeeded\"\":%d,\"\"end_time\"\":%lu,\"\"error_edge\"\":%lu,\"\"error_type\"\":%d,\"\"route\"\":[", attempt->attempts, attempt->is_succeeded, attempt->end_time, attempt->error_edge_id, attempt->error_type);
            for (j = 0; j < array_len(attempt->route); j++) {
                struct edge_snapshot* edge_snapshot = array_get(attempt->route, j);
                edge = array_get(network->edges, edge_snapshot->id);
                channel = array_get(network->channels, edge->channel_id);
                fprintf(csv_payment_output,"{\"\"edge_id\"\":%lu,\"\"from_node_id\"\":%lu,\"\"to_node_id\"\":%lu,\"\"sent_amt\"\":%lu,\"\"edge_cap\"\":%lu,\"\"channel_cap\"\":%lu,", edge_snapshot->id, edge->from_node_id, edge->to_node_id, edge_snapshot->sent_amt, edge_snapshot->balance, channel->capacity);
                if(edge_snapshot->is_in_group) fprintf(csv_payment_output, "\"\"group_cap\"\":%lu,", edge_snapshot->group_cap);
                else fprintf(csv_payment_output,"\"\"group_cap\"\":null,");
                if(edge_snapshot->does_channel_update_exist) fprintf(csv_payment_output,"\"\"channel_update\"\":%lu}", edge_snapshot->last_channle_update_value);
                else fprintf(csv_payment_output,"\"\"channel_update\"\":}");
                if (j != array_len(attempt->route) - 1) fprintf(csv_payment_output, ",");
            }
            fprintf(csv_payment_output, "]}");
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
  pay_params->inverse_payment_rate = pay_params->average_amount = 0.0;
  pay_params->n_payments = 0;
  pay_params->payments_from_file = 0;
  strcpy(pay_params->payments_filename, "\0");
  pay_params->mpp = 0;
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
      pay_params->average_amount = strtod(value, NULL);
    }
    else if(strcmp(parameter, "mpp")==0){
      pay_params->mpp = strtoul(value, NULL, 10);
    }
    else if(strcmp(parameter, "log_all_events")==0){
        if(strcmp(value, "true")==0)
            pay_params->log_all_events=1;
        else if(strcmp(value, "false")==0)
            pay_params->log_all_events=0;
        else{
            fprintf(stderr, "ERROR: wrong value of parameter <%s> in <cloth_input.txt>. Possible values are <true> or <false>\n", parameter);
            fclose(input_file);
            exit(-1);
        }
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


/* process stats of payments that were split (mpp payments) */
void post_process_payment_stats(struct array* payments){
  long i;
  struct payment* payment, *shard1, *shard2;
  for(i = 0; i < array_len(payments); i++){
    payment = array_get(payments, i);
    if(payment->id == -1) continue;
    if(!has_shards(payment)) continue;
    shard1 = array_get(payments, payment->shards_id[0]);
    shard2 = array_get(payments, payment->shards_id[1]);
    payment->end_time = shard1->end_time > shard2->end_time ? shard1->end_time : shard2->end_time;
    payment->is_success = shard1->is_success && shard2->is_success ? 1 : 0;
    payment->no_balance_count = shard1->no_balance_count + shard2->no_balance_count;
    payment->offline_node_count = shard1->offline_node_count + shard2->offline_node_count;
    payment->is_timeout = shard1->is_timeout || shard2->is_timeout ? 1 : 0;
    payment->attempts = shard1->attempts + shard2->attempts;
    if(shard1->route != NULL && shard2->route != NULL){
      payment->route = array_len(shard1->route->route_hops) > array_len(shard2->route->route_hops) ? shard1->route : shard2->route;
      payment->route->total_fee = shard1->route->total_fee + shard2->route->total_fee;
    }
    else{
      payment->route = NULL;
    }
    //a trick to avoid processing already processed shards
    shard1->id = -1;
    shard2->id = -1;
  }
}


gsl_rng* initialize_random_generator(){
  gsl_rng_env_setup();
  return gsl_rng_alloc (gsl_rng_default);
}


uint64_t calc_simulation_env_hash(struct network* network, struct array* payments, struct network_params* net_params, struct simulation* simulation){

    uint64_t hash_network_settings = 0;
    {
        hash_network_settings += *SHA512Hash((uint8_t*)(&(net_params->n_nodes)), sizeof(long));
        hash_network_settings += *SHA512Hash((uint8_t*)(&(net_params->n_channels)), sizeof(long));
        hash_network_settings += *SHA512Hash((uint8_t*)(&(net_params->capacity_per_channel)), sizeof(long));
        hash_network_settings += *SHA512Hash((uint8_t*)(&(net_params->faulty_node_prob)), sizeof(double));
        hash_network_settings += *SHA512Hash((uint8_t*)(&(net_params->network_from_file)), sizeof(unsigned int));
        hash_network_settings += *SHA512Hash((uint8_t*)(&(net_params->routing_method)), sizeof(enum routing_method));
//        hash_network_settings += *SHA512Hash((uint8_t*)(&(net_params->group_cap_update)), sizeof(unsigned int));
//        hash_network_settings += *SHA512Hash((uint8_t*)(&(net_params->group_size)), sizeof(int));
//        hash_network_settings += *SHA512Hash((uint8_t*)(&(net_params->group_limit_rate)), sizeof(float));
        for(int i = 0; net_params->nodes_filename[i] != '\0'; i++) hash_network_settings += *SHA512Hash((uint8_t*)(&(net_params->nodes_filename[i])), sizeof(char));
        for(int i = 0; net_params->channels_filename[i] != '\0'; i++) hash_network_settings += *SHA512Hash((uint8_t*)(&(net_params->channels_filename[i])), sizeof(char));
        for(int i = 0; net_params->edges_filename[i] != '\0'; i++) hash_network_settings += *SHA512Hash((uint8_t*)(&(net_params->edges_filename[i])), sizeof(char));
    }
    printf("hash_network_settings=%lu\n", hash_network_settings);

    uint64_t hash_network_channels = 0;
    for(long channel_id = 0; channel_id < array_len(network->channels); channel_id++){
        struct channel* c = array_get(network->channels, channel_id);
        hash_network_channels += *SHA512Hash((uint8_t*)(&(c->id)), sizeof(uint64_t));
        hash_network_channels += *SHA512Hash((uint8_t*)(&(c->node1)), sizeof(uint64_t));
        hash_network_channels += *SHA512Hash((uint8_t*)(&(c->node2)), sizeof(uint64_t));
        hash_network_channels += *SHA512Hash((uint8_t*)(&(c->edge1)), sizeof(uint64_t));
        hash_network_channels += *SHA512Hash((uint8_t*)(&(c->edge2)), sizeof(uint64_t));
        hash_network_channels += *SHA512Hash((uint8_t*)(&(c->capacity)), sizeof(uint64_t));
        hash_network_channels += *SHA512Hash((uint8_t*)(&(c->is_closed)), sizeof(uint32_t));
    }
    printf("hash_network_channels=%lu\n", hash_network_channels);

    uint64_t hash_network_nodes = 0;
    for(long node_id = 0; node_id < array_len(network->nodes); node_id++){
        struct node* n = array_get(network->nodes, node_id);
        hash_network_nodes += *SHA512Hash((uint8_t*)(&(n->id)), sizeof(uint64_t));
        for(long i = 0; i < array_len(n->open_edges); i++){
            struct edge* e = array_get(n->open_edges, i);
            hash_network_nodes += *SHA512Hash((uint8_t*)(&(e->id)), sizeof(uint64_t));
        }
    }
    printf("hash_network_nodes=%lu\n", hash_network_nodes);

    uint64_t hash_network_edges = 0;
    for(long edge_id = 0; edge_id < array_len(network->edges); edge_id++){
        struct edge* e = array_get(network->edges, edge_id);
        hash_network_edges += *SHA512Hash((uint8_t*)(&(e->id)), sizeof(uint64_t));
        hash_network_edges += *SHA512Hash((uint8_t*)(&(e->channel_id)), sizeof(uint64_t));
        hash_network_edges += *SHA512Hash((uint8_t*)(&(e->from_node_id)), sizeof(uint64_t));
        hash_network_edges += *SHA512Hash((uint8_t*)(&(e->to_node_id)), sizeof(uint64_t));
        hash_network_edges += *SHA512Hash((uint8_t*)(&(e->counter_edge_id)), sizeof(uint64_t));
        hash_network_edges += *SHA512Hash((uint8_t*)(&(e->policy.fee_base)), sizeof(uint64_t));
        hash_network_edges += *SHA512Hash((uint8_t*)(&(e->policy.min_htlc)), sizeof(uint64_t));
        hash_network_edges += *SHA512Hash((uint8_t*)(&(e->policy.timelock)), sizeof(uint64_t));
        hash_network_edges += *SHA512Hash((uint8_t*)(&(e->balance)), sizeof(uint64_t));
        hash_network_edges += *SHA512Hash((uint8_t*)(&(e->is_closed)), sizeof(uint32_t));
        hash_network_edges += *SHA512Hash((uint8_t*)(&(e->tot_flows)), sizeof(uint64_t));
    }
    printf("hash_network_edges=%lu\n", hash_network_edges);

    uint64_t hash_network_payments = 0;
    for(long payment_id = 0; payment_id < array_len(payments); payment_id++){
        struct payment* p = array_get(payments, payment_id);
        hash_network_payments += *SHA512Hash((uint8_t*)(&(p->id)), sizeof(uint64_t));
        hash_network_payments += *SHA512Hash((uint8_t*)(&(p->sender)), sizeof(uint64_t));
        hash_network_payments += *SHA512Hash((uint8_t*)(&(p->receiver)), sizeof(uint64_t));
        hash_network_payments += *SHA512Hash((uint8_t*)(&(p->amount)), sizeof(uint64_t));
    }
    printf("hash_payments=%lu\n", hash_network_payments);

    uint64_t hash_seed = 0;
    gsl_rng* random_generator = initialize_random_generator();
    for(int i = 0; i < 100; i++){
        double r = gsl_ran_ugaussian(random_generator);
        hash_seed += *SHA512Hash((uint8_t*)(&r), sizeof(double));
    }
    printf("hash_seed=%lu\n", hash_seed);

    return hash_network_settings + hash_network_channels + hash_network_nodes + hash_network_edges + hash_network_payments + hash_seed;
}


void write_dijkstra_cache(char* dijkstra_cache_name, struct network* network, struct array* payments, struct network_params* net_params, struct simulation* simulation){
    FILE* dijkstra_cache = fopen(dijkstra_cache_name, "w");
    if(dijkstra_cache != NULL){
        uint64_t hash = calc_simulation_env_hash(network, payments, net_params, simulation);
        fprintf(dijkstra_cache, "%lu\n", hash);
        for(long payment_id = 0; payment_id < array_len(payments); payment_id++){
            if(paths[payment_id] == NULL) continue;
            struct array* path_hops = paths[payment_id];
            for(long j = 0; j < array_len(path_hops); j++){
                struct path_hop* hop = array_get(path_hops, j);
                fprintf(dijkstra_cache, "%ld,%ld,%ld,%ld\n", payment_id, hop->sender, hop->receiver, hop->edge);
            }
        }
        fclose(dijkstra_cache);
    }
}


void inflate_path_from_cache(FILE* dijkstra_cache){
    while(1) {
        long payment_id;
        struct path_hop* hop = malloc(sizeof(struct route_hop));
        if(fscanf(dijkstra_cache, "%ld,%ld,%ld,%ld\n", &payment_id, &(hop->sender), &(hop->receiver), &(hop->edge)) == EOF) break;
        if(paths[payment_id] == NULL) paths[payment_id] = array_initialize(10);
        paths[payment_id] = array_insert(paths[payment_id], hop);
    }
    fclose(dijkstra_cache);
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
  char dijkstra_cache_name[256];

  if(argc < 2) {
    fprintf(stderr, "ERROR cloth.c: please specify the output directory\n");
    return -1;
  }
  if(argc == 3){
      strcpy(dijkstra_cache_name, argv[2]);
  }else{
      strcpy(dijkstra_cache_name, "dijkstra_cache");
  }
  strcpy(output_dir_name, argv[1]);

  read_input(&net_params, &pay_params);

  simulation = malloc(sizeof(struct simulation));

  simulation->random_generator = initialize_random_generator();
  printf("NETWORK INITIALIZATION\n");
  network = initialize_network(net_params, simulation->random_generator);
  n_nodes = array_len(network->nodes);
  n_edges = array_len(network->edges);

    // add edge which is not a member of any group to group_add_queue
    struct element* group_add_queue = NULL;
    if(net_params.routing_method == GROUP_ROUTING) {
        for (int i = 0; i < n_edges; i++) {
            group_add_queue = list_insert_sorted_position(group_add_queue, array_get(network->edges, i), (long (*)(void *)) get_edge_balance);
        }
        group_add_queue = construct_groups(simulation, group_add_queue, network, net_params);
    }

  printf("PAYMENTS INITIALIZATION\n");
  payments = initialize_payments(pay_params,  n_nodes, simulation->random_generator);

  printf("EVENTS INITIALIZATION\n");
  simulation->events = initialize_events(payments);
  initialize_dijkstra(n_nodes, n_edges, payments);

  printf("INITIAL DIJKSTRA THREADS EXECUTION\n");
  clock_gettime(CLOCK_MONOTONIC, &start);
  FILE* dijkstra_cache = fopen(dijkstra_cache_name, "r");
  // use dijkstra cache
  if(dijkstra_cache == NULL){
      printf("no cache file\n");
      run_dijkstra_threads(network, payments, 0, net_params.routing_method);
      write_dijkstra_cache(dijkstra_cache_name, network, payments, &net_params, simulation);
      printf("cache file created\n");
  } else {
      uint64_t hash = calc_simulation_env_hash(network, payments, &net_params, simulation);
      uint64_t dijkstra_cache_env_hash;
      printf("cache file found\n");
      fscanf(dijkstra_cache, "%lu\n", &dijkstra_cache_env_hash);
      if(hash == dijkstra_cache_env_hash){
          inflate_path_from_cache(dijkstra_cache);
      }else{
          fclose(dijkstra_cache);
          printf("invalid cache\n");
          run_dijkstra_threads(network, payments, 0, net_params.routing_method);
          write_dijkstra_cache(dijkstra_cache_name, network, payments, &net_params, simulation);
          printf("cache file updated\n");
      }
  }
  clock_gettime(CLOCK_MONOTONIC, &finish);
  time_spent_thread = finish.tv_sec - start.tv_sec;
  printf("Time consumed by initial dijkstra executions: %ld s\n", time_spent_thread);

  printf("EXECUTION OF THE SIMULATION\n");

  FILE *csv_events;
  if(pay_params.log_all_events) {
      char csv_events_filename[512];
      strcpy(csv_events_filename, output_dir_name);
      strcat(csv_events_filename, "events.csv");
      csv_events = fopen(csv_events_filename, "w");
      if (csv_events == NULL) {
          printf("ERROR cannot open events.csv\n");
          exit(-1);
      }
      fprintf(csv_events, "time,pmt_id,event_type,sender,receiver,amt,error,attempts,elapsed_time,route\n");
  }

  /* core of the discrete-event simulation: extract next event, advance simulation time, execute the event */
  begin = clock();
  simulation->current_time = 1;
  long completed_payments = 0;
  while(heap_len(simulation->events) != 0) {
    event = heap_pop(simulation->events, compare_event);

    if(pay_params.log_all_events) {
        // write event to file
        fprintf(csv_events, "%lu,%lu,%d,%lu,%lu,%lu,%d,%d,", simulation->current_time, event->payment->id, event->type,
                event->payment->sender, event->payment->receiver, event->payment->amount, event->payment->error.type,
                event->payment->attempts);
        if (event->type != 0) {
            fprintf(csv_events, "%lu,", simulation->current_time - event->payment->start_time);
        } else {
            fprintf(csv_events, ",");
        }
        if (event->payment->route != NULL) {
            for (int i = 0; i < array_len(event->payment->route->route_hops); i++) {
                struct route_hop *hop = array_get(event->payment->route->route_hops, i);
                fprintf(csv_events, "%lu", hop->edge_id);
                if (i < array_len(event->payment->route->route_hops) - 1) fprintf(csv_events, "-");
                else fprintf(csv_events, "\n");
            }
        } else {
            fprintf(csv_events, "\n");
        }
    }

    simulation->current_time = event->time;
    switch(event->type){
    case FINDPATH:
      find_path(event, simulation, network, &payments, pay_params.mpp, net_params.routing_method, net_params);
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
      open_channel(network, simulation->random_generator);
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

  if(pay_params.mpp)
    post_process_payment_stats(payments);

  time_spent = (double) (end - begin)/CLOCKS_PER_SEC;
  printf("Time consumed by simulation events: %lf s\n", time_spent);

  write_output(network, payments, output_dir_name);

    list_free(group_add_queue);
    free(simulation->random_generator);
    heap_free(simulation->events);
  free(simulation);

  return 0;
}
