#include <string.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_rng.h>
#include "../include/network.h"
#include "../include/array.h"
#include "../include/utils.h"


/* Functions in this file generate a payment-channel network where to simulate the execution of payments */


struct node* new_node(long id) {
  struct node* node;
  node = malloc(sizeof(struct node));
  node->id=id;
  node->open_edges = array_initialize(10);
  node->results = NULL;
  node->explored = 0;
  return node;
}


struct channel* new_channel(long id, long direction1, long direction2, long node1, long node2, uint64_t capacity) {
  struct channel* channel;
  channel = malloc(sizeof(struct channel));
  channel->id = id;
  channel->edge1 = direction1;
  channel->edge2 = direction2;
  channel->node1 = node1;
  channel->node2 = node2;
  channel->capacity = capacity;
  channel->is_closed = 0;
  return channel;
}


//struct edge* new_edge(long id, long channel_id, long counter_edge_id, long from_node_id, long to_node_id, uint64_t balance, struct policy policy, uint64_t channel_capacity){
struct edge* new_edge(long id, long channel_id, long counter_edge_id, long from_node_id, long to_node_id, uint64_t balance, struct policy policy, uint64_t channel_capacity){
  struct edge* edge;
  edge = malloc(sizeof(struct edge));
  edge->id = id;
  edge->channel_id = channel_id;
  edge->from_node_id = from_node_id;
  edge->to_node_id = to_node_id;
  edge->counter_edge_id = counter_edge_id;
  edge->policy = policy;
  edge->balance = balance;
  edge->is_closed = 0;
  edge->tot_flows = 0;
  edge->group = NULL;
  struct channel_update* channel_update = malloc(sizeof(struct channel_update));
  channel_update->htlc_maximum_msat = channel_capacity;
  edge->channel_updates = push(NULL, channel_update);
  return edge;
}


/* after generating a network, write it in csv files "nodes.csv" "edges.csv" "channels.csv" */
void write_network_files(struct network* network){
  FILE* nodes_output_file, *edges_output_file, *channels_output_file;
  long i;
  struct node* node;
  struct channel* channel;
  struct edge* edge;

  nodes_output_file = fopen("nodes.csv", "w");
  if(nodes_output_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file <%s>\n", "nodes.csv");
    exit(-1);
  }
  fprintf(nodes_output_file, "id\n");
  channels_output_file = fopen("channels.csv", "w");
  if(channels_output_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file <%s>\n", "channels.csv");
    fclose(nodes_output_file);
    exit(-1);
  }
  fprintf(channels_output_file, "id,edge1_id,edge2_id,node1_id,node2_id,capacity\n");
  edges_output_file = fopen("edges.csv", "w");
  if(edges_output_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file <%s>\n", "edges.csv");
    fclose(nodes_output_file);
    fclose(channels_output_file);
    exit(-1);
  }
  fprintf(edges_output_file, "id,channel_id,counter_edge_id,from_node_id,to_node_id,balance,fee_base,fee_proportional,min_htlc,timelock\n");

  for(i=0; i<array_len(network->nodes); i++){
    node = array_get(network->nodes, i);
    fprintf(nodes_output_file, "%ld\n", node->id);
  }

  for(i=0; i<array_len(network->channels); i++){
    channel = array_get(network->channels, i);
    fprintf(channels_output_file, "%ld,%ld,%ld,%ld,%ld,%ld\n", channel->id, channel->edge1, channel->edge2, channel->node1, channel->node2, channel->capacity);
  }

  for(i=0; i<array_len(network->edges); i++){
    edge = array_get(network->edges, i);
    fprintf(edges_output_file, "%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%d\n", edge->id, edge->channel_id, edge->counter_edge_id, edge->from_node_id, edge->to_node_id, edge->balance, (edge->policy).fee_base, (edge->policy).fee_proportional, (edge->policy).min_htlc, (edge->policy).timelock);
  }

  fclose(nodes_output_file);
  fclose(edges_output_file);
  fclose(channels_output_file);
}


void update_probability_per_node(double *probability_per_node, int *channels_per_node, long n_nodes, long node1_id, long node2_id, long tot_channels){
  long i;
  channels_per_node[node1_id] += 1;
  channels_per_node[node2_id] += 1;
  for(i=0; i<n_nodes; i++)
    probability_per_node[i] = ((double)channels_per_node[i])/tot_channels;
}

/* generate a channel (connecting node1_id and node2_id) with random values */
void generate_random_channel(struct channel channel_data, uint64_t mean_channel_capacity, struct network* network, gsl_rng*random_generator) {
  uint64_t capacity, edge1_balance, edge2_balance;
  struct policy edge1_policy, edge2_policy;
  double min_htlcP[]={0.7, 0.2, 0.05, 0.05}, fraction_capacity;
  gsl_ran_discrete_t* min_htlc_discrete;
  struct channel* channel;
  struct edge* edge1, *edge2;
  struct node* node;

  capacity = fabs(mean_channel_capacity + gsl_ran_ugaussian(random_generator));
  channel = new_channel(channel_data.id, channel_data.edge1, channel_data.edge2, channel_data.node1, channel_data.node2, capacity*1000);

  fraction_capacity = gsl_rng_uniform(random_generator);
  edge1_balance = fraction_capacity*((double) capacity);
  edge2_balance = capacity - edge1_balance;
  //multiplied by 1000 to convert satoshi to millisatoshi
  edge1_balance*=1000;
  edge2_balance*=1000;

  min_htlc_discrete = gsl_ran_discrete_preproc(4, min_htlcP);
  edge1_policy.fee_base = gsl_rng_uniform_int(random_generator, MAXFEEBASE - MINFEEBASE) + MINFEEBASE;
  edge1_policy.fee_proportional = (gsl_rng_uniform_int(random_generator, MAXFEEPROP-MINFEEPROP)+MINFEEPROP);
  edge1_policy.timelock = gsl_rng_uniform_int(random_generator, MAXTIMELOCK-MINTIMELOCK)+MINTIMELOCK;
  edge1_policy.min_htlc = gsl_pow_int(10, gsl_ran_discrete(random_generator, min_htlc_discrete));
  edge1_policy.min_htlc = edge1_policy.min_htlc == 1 ? 0 : edge1_policy.min_htlc;
  edge2_policy.fee_base = gsl_rng_uniform_int(random_generator, MAXFEEBASE - MINFEEBASE) + MINFEEBASE;
  edge2_policy.fee_proportional = (gsl_rng_uniform_int(random_generator, MAXFEEPROP-MINFEEPROP)+MINFEEPROP);
  edge2_policy.timelock = gsl_rng_uniform_int(random_generator, MAXTIMELOCK-MINTIMELOCK)+MINTIMELOCK;
  edge2_policy.min_htlc = gsl_pow_int(10, gsl_ran_discrete(random_generator, min_htlc_discrete));
  edge2_policy.min_htlc = edge2_policy.min_htlc == 1 ? 0 : edge2_policy.min_htlc;

  edge1 = new_edge(channel_data.edge1, channel_data.id, channel_data.edge2, channel_data.node1, channel_data.node2, edge1_balance, edge1_policy, channel_data.capacity);
  edge2 = new_edge(channel_data.edge2, channel_data.id, channel_data.edge1, channel_data.node2, channel_data.node1, edge2_balance, edge2_policy, channel_data.capacity);

  network->channels = array_insert(network->channels, channel);
  network->edges = array_insert(network->edges, edge1);
  network->edges = array_insert(network->edges, edge2);

  node = array_get(network->nodes, channel_data.node1);
  node->open_edges = array_insert(node->open_edges, &(edge1->id));
  node = array_get(network->nodes, channel_data.node2);
  node->open_edges = array_insert(node->open_edges, &(edge2->id));
}


/* generate a random payment-channel network;
   the model of the network is a snapshot of the Lightning Network (files "nodes_ln.csv", "channels_ln.csv");
   starting from this network, a random network is generated using the scale-free network model */
struct network* generate_random_network(struct network_params net_params, gsl_rng* random_generator){
  FILE* nodes_input_file, *channels_input_file;
  char row[256];
  long node_id_counter=0, id, channel_id_counter=0, tot_nodes, i, tot_channels, node_to_connect_id, edge_id_counter=0, j;
  double *probability_per_node;
  int *channels_per_node;
  struct network* network;
  struct node* node;
  gsl_ran_discrete_t* connection_probability;
  struct channel channel;

  nodes_input_file = fopen("nodes_ln.csv", "r");
  if(nodes_input_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file <%s>\n", "nodes_ln.csv");
    exit(-1);
  }
  channels_input_file = fopen("channels_ln.csv", "r");
  if(channels_input_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file <%s>\n", "channels_ln.csv");
    fclose(nodes_input_file);
    exit(-1);
  }

  network = (struct network*) malloc(sizeof(struct network));
  network->nodes = array_initialize(1000);
  network->channels = array_initialize(1000);
  network->edges = array_initialize(2000);

  fgets(row, 256, nodes_input_file);
  while(fgets(row, 256, nodes_input_file)!=NULL) {
    sscanf(row, "%ld,%*d", &id);
    node = new_node(id);
    network->nodes = array_insert(network->nodes, node);
    node_id_counter++;
  }
  tot_nodes = node_id_counter + net_params.n_nodes;
  if(tot_nodes == 0){
    fprintf(stderr, "ERROR: it is not possible to generate a network with 0 nodes\n");
    fclose(nodes_input_file);
    fclose(channels_input_file);
    exit(-1);
  }

  channels_per_node = malloc(sizeof(int)*(tot_nodes));
  for(i = 0; i < tot_nodes; i++){
    channels_per_node[i] = 0;
  }

  fgets(row, 256, channels_input_file);
  while(fgets(row, 256, channels_input_file)!=NULL) {
    sscanf(row, "%ld,%ld,%ld,%ld,%ld,%*d,%*d", &(channel.id), &(channel.edge1), &(channel.edge2), &(channel.node1), &(channel.node2));
    generate_random_channel(channel, net_params.capacity_per_channel, network, random_generator);
    channels_per_node[channel.node1] += 1;
    channels_per_node[channel.node2] += 1;
    ++channel_id_counter;
    edge_id_counter+=2;
  }
  tot_channels = channel_id_counter;
  if(tot_channels == 0){
    fprintf(stderr, "ERROR: it is not possible to generate a network with 0 channels\n");
    fclose(nodes_input_file);
    fclose(channels_input_file);
    exit(-1);
  }

  probability_per_node = malloc(sizeof(double)*tot_nodes);
  for(i=0; i<tot_nodes; i++){
    probability_per_node[i] = ((double)channels_per_node[i])/tot_channels;
  }

  /* scale-free algorithm that creates a network starting from an existing network;
     the probability of connecting nodes is directly proprotional to the number of channels that a node has already open */
  for(i=0; i<net_params.n_nodes; i++){
    node = new_node(node_id_counter);
    network->nodes = array_insert(network->nodes, node);
    for(j=0; j<net_params.n_channels; j++){
      connection_probability = gsl_ran_discrete_preproc(node_id_counter, probability_per_node);
      node_to_connect_id = gsl_ran_discrete(random_generator, connection_probability);
      channel.id = channel_id_counter;
      channel.edge1 = edge_id_counter;
      channel.edge2 = edge_id_counter + 1;
      channel.node1 = node->id;
      channel.node2 = node_to_connect_id;
      generate_random_channel(channel, net_params.capacity_per_channel, network, random_generator);
      channel_id_counter++;
      edge_id_counter += 2;
      update_probability_per_node(probability_per_node, channels_per_node, tot_nodes, node->id, node_to_connect_id, channel_id_counter);
    }
    ++node_id_counter;
  }

  fclose(nodes_input_file);
  fclose(channels_input_file);
  free(channels_per_node);
  free(probability_per_node);

  write_network_files(network);

  return network;
}


/* generate a payment-channel network from input files */
struct network* generate_network_from_files(char nodes_filename[256], char channels_filename[256], char edges_filename[256]) {
  char row[2048];
  struct node* node;
  long id, direction1, direction2, node_id1, node_id2, channel_id, other_direction;
  struct policy policy;
  uint64_t capacity, balance;
  struct channel* channel;
  struct edge* edge;
  struct network* network;
  FILE *nodes_file, *channels_file, *edges_file;

  nodes_file = fopen(nodes_filename, "r");
  if(nodes_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file <%s>\n", nodes_filename);
    exit(-1);
  }
  channels_file = fopen(channels_filename, "r");
  if(channels_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file <%s>\n", channels_filename);
    fclose(nodes_file);
    exit(-1);
  }
  edges_file = fopen(edges_filename, "r");
  if(edges_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file <%s>\n", edges_filename);
    fclose(nodes_file);
    fclose(channels_file);
    exit(-1);
  }

  network = (struct network*) malloc(sizeof(struct network));
  network->nodes = array_initialize(1000);
  network->channels = array_initialize(1000);
  network->edges = array_initialize(2000);

  fgets(row, 2048, nodes_file);
  while(fgets(row, 2048, nodes_file)!=NULL) {
    sscanf(row, "%ld", &id);
    node = new_node(id);
    network->nodes = array_insert(network->nodes, node);
  }
  fclose(nodes_file);

  fgets(row, 2048, channels_file);
  while(fgets(row, 2048, channels_file)!=NULL) {
    sscanf(row, "%ld,%ld,%ld,%ld,%ld,%ld", &id, &direction1, &direction2, &node_id1, &node_id2, &capacity);
    channel = new_channel(id, direction1, direction2, node_id1, node_id2, capacity);
    network->channels = array_insert(network->channels, channel);
  }
  fclose(channels_file);


  fgets(row, 2048, edges_file);
  while(fgets(row, 2048, edges_file)!=NULL) {
    sscanf(row, "%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%d", &id, &channel_id, &other_direction, &node_id1, &node_id2, &balance, &policy.fee_base, &policy.fee_proportional, &policy.min_htlc, &policy.timelock);
    channel = array_get(network->channels, channel_id);
    edge = new_edge(id, channel_id, other_direction, node_id1, node_id2, balance, policy, channel->capacity);
    network->edges = array_insert(network->edges, edge);
    node = array_get(network->nodes, node_id1);
    node->open_edges = array_insert(node->open_edges, &(edge->id));
  }
  fclose(edges_file);

  return network;
}


struct network* initialize_network(struct network_params net_params, gsl_rng* random_generator) {
  struct network* network;
  double faulty_prob[2];
  long n_nodes;
  long i, j;
  struct node* node;

  if(net_params.network_from_file)
    network = generate_network_from_files(net_params.nodes_filename, net_params.channels_filename, net_params.edges_filename);
  else
    network = generate_random_network(net_params, random_generator);

  faulty_prob[0] = 1-net_params.faulty_node_prob;
  faulty_prob[1] = net_params.faulty_node_prob;
  network->faulty_node_prob = gsl_ran_discrete_preproc(2, faulty_prob);

  n_nodes = array_len(network->nodes);
  for(i=0; i<n_nodes; i++){
    node = array_get(network->nodes, i);
    node->results = (struct element**) malloc(n_nodes*sizeof(struct element*));
    for(j=0; j<n_nodes; j++)
      node->results[j] = NULL;
  }

  network->groups = array_initialize(1000);

  return  network;
}

/* open a new channel during the simulation */
/* currenlty NOT USED */
void open_channel(struct network* network, gsl_rng* random_generator){
  struct channel channel;
  channel.id = array_len(network->channels);
  channel.edge1 = array_len(network->edges);
  channel.edge2 = array_len(network->edges) + 1;
  channel.node1 = gsl_rng_uniform_int(random_generator, array_len(network->nodes));
  do{
    channel.node2 = gsl_rng_uniform_int(random_generator, array_len(network->nodes));
  } while(channel.node2==channel.node1);
  generate_random_channel(channel, 1000, network, random_generator);
}

struct element* update_group(struct group* group, struct network_params net_params, uint64_t current_time, struct element* group_add_queue, long triggered_node_id, unsigned char is_init_update, FILE* csv_group_output) {

    // update group cap
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;
    for (int i = 0; i < array_len(group->edges); i++) {
        struct edge* edge = array_get(group->edges, i);
        if(edge->balance < min) min = edge->balance;
        if(edge->balance > max) max = edge->balance;
    }
    group->max_cap = max;
    group->min_cap = min;
    if(net_params.group_cap_update) {
        group->group_cap = min;
    }else{
        group->group_cap = group->min_cap_limit;
    }

    struct group_update* group_update = malloc(sizeof(struct group_update));
    group_update->group_id = group->id;
    group_update->group_cap = group->group_cap;
    group_update->time = current_time;
    group_update->triggered_node_id = triggered_node_id;
    group_update->balances_of_edge_in_queue_snapshot = NULL;
    group_update->type = UPDATE;

    // close if edge capacity is out of limit
    if (group->min_cap < group->min_cap_limit || group->max_cap_limit < group->max_cap) {
        group->is_closed = current_time;
        group_update->type = CLOSE;

        // add group members to group_add_queue
        // broadcast group_req msg
        for(int i = 0; i < array_len(group->edges); i++){
            struct edge* edge = array_get(group->edges, i);
            edge->group = NULL;
            group_add_queue = push(group_add_queue, edge);

            // take snapshot of the group
            struct edge* copy = malloc(sizeof(struct edge));
            copy->id = edge->id;
            copy->channel_id = edge->channel_id;
            copy->from_node_id = edge->from_node_id;
            copy->to_node_id = edge->to_node_id;
            copy->counter_edge_id = edge->counter_edge_id;
            copy->policy = edge->policy;
            copy->balance = edge->balance;
            copy->is_closed = edge->is_closed;
            copy->tot_flows = edge->tot_flows;
            copy->group = NULL;
            copy->channel_updates = NULL;
            for(struct element* channel_update_i = edge->channel_updates; channel_update_i != NULL; channel_update_i = channel_update_i->next){
                struct channel_update* channel_update = channel_update_i->data;
                struct channel_update* channel_update_copy = malloc(sizeof(struct channel_update));
                channel_update_copy->htlc_maximum_msat = channel_update->htlc_maximum_msat;
                copy->channel_updates = push(copy->channel_updates, channel_update_copy);
            }
            group->edges->element[i] = copy;
        }
    }
    if(is_init_update) group_update->type = CONSTRUCT;

    // take snapshot of group_add_queue
    for(struct element* iterator = group_add_queue; iterator != NULL; iterator = iterator->next){
        struct edge* requesting_edge = iterator->data;
        uint64_t balance = requesting_edge->balance;
        group_update->balances_of_edge_in_queue_snapshot = push(group_add_queue, &balance);
    }

    // if this group is not simulation's initial construction, write to log file
    if(net_params.log_broadcast_msg && current_time != 0) {
        write_group_update(csv_group_output, group_update);
    }
    free(group_update);

    return group_add_queue;
}

struct element* construct_group(struct element* group_add_queue, struct network *network, gsl_rng *random_generator, struct network_params net_params, uint64_t current_time, FILE* csv_group_update){

    if(group_add_queue == NULL) return group_add_queue;
    struct element* first_edge_i;
    for(first_edge_i = group_add_queue; first_edge_i != NULL; first_edge_i = first_edge_i->next){
        struct edge* first_edge = first_edge_i->data;

        // if edge is already member of group, skip
        if(first_edge->group != NULL) continue;

        // init group
        struct group* group = malloc(sizeof(struct group));
        group->edges = array_initialize(net_params.group_size);
        group->edges = array_insert(group->edges, first_edge);
        group->max_cap_limit = first_edge->balance + (uint64_t)((float)first_edge->balance * net_params.group_limit_rate);
        group->min_cap_limit = first_edge->balance - (uint64_t)((float)first_edge->balance * net_params.group_limit_rate);
        group->id = array_len(network->groups);
        group->is_closed = 0;

        // select edge whose balance is in group allowance
        for(struct element *next_edge_i = group_add_queue; next_edge_i != NULL; next_edge_i = next_edge_i->next){
            struct edge* next_edge = next_edge_i->data;

            // if next_edge is already used by other group, skip
            if(next_edge->group != NULL) continue;

            // if next_edge is counter_edge of first_edge, skip
            struct edge *prev_edge = array_get(group->edges, array_len(group->edges) -1);
            if(next_edge->id == prev_edge->counter_edge_id) continue;

            // if next_edge is already in group, skip
            char is_already_exits_in_group = 0;
            for(int l = 0; l < array_len(group->edges); l++){
                struct edge* group_edge = array_get(group->edges, l);
                if(group_edge->id == next_edge->id){
                    is_already_exits_in_group = 1;
                    break;
                }
            }
            if(is_already_exits_in_group) continue;

            // if next_edge is directory connected to first_edge, skip
            if (first_edge->to_node_id == next_edge->to_node_id ||
                first_edge->to_node_id == next_edge->from_node_id ||
                first_edge->from_node_id == next_edge->to_node_id ||
                first_edge->from_node_id == next_edge->from_node_id)
                continue;

            // if next_edge's balance satisfy allowance, add next_edge to group
            if(group->min_cap_limit <= next_edge->balance && next_edge->balance <= group->max_cap_limit){
                group->edges = array_insert(group->edges, next_edge);

                // if group member is full, end
                if(array_len(group->edges) >= net_params.group_size) break;
            }
        }

        // register group to network
        if(array_len(group->edges) == net_params.group_size) {
            network->groups = array_insert(network->groups, group);
            update_group(group, net_params, current_time, group_add_queue, first_edge->id, 1, csv_group_update);
            for (int j = 0; j < array_len(group->edges); j++) {
                struct edge *edge = array_get(group->edges, j);
                group_add_queue = list_delete(group_add_queue, &first_edge_i, edge, (int (*)(void *, void *)) edge_equal);
                edge->group = group;
            }
            if(first_edge_i == NULL) break;
        }
    }
    return group_add_queue;
}

int edge_equal(struct edge* e1, struct edge* e2){
    return e1->id == e2->id;
}