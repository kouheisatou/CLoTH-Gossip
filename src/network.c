#include <string.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_rng.h>
#include "../include/network.h"
#include "../include/array.h"


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


struct edge* new_edge(long id, long channel_id, long counter_edge_id, long from_node_id, long to_node_id, uint64_t balance, struct policy policy){
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

  edge1 = new_edge(channel_data.edge1, channel_data.id, channel_data.edge2, channel_data.node1, channel_data.node2, edge1_balance, edge1_policy);
  edge2 = new_edge(channel_data.edge2, channel_data.id, channel_data.edge1, channel_data.node2, channel_data.node1, edge2_balance, edge2_policy);

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
    edge = new_edge(id, channel_id, other_direction, node_id1, node_id2, balance, policy);
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

//    construct_loop_group(network);
    construct_groups_close_capacity(network, random_generator);
//    construct_groups_randomly(network, random_generator);

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

void construct_groups_randomly(struct network *network, gsl_rng *random_generator) {

    // group construction policy
    int group_size_mean = 10;
    int group_size_variance = 5;
    int group_size_minimum_limit = 5;

    network->groups = array_initialize(1000);
    long group_seq = 0L;

    for (long i = 0; i < array_len(network->nodes); i++) {

        // show progress
        float progress = (float)i / (float)array_len(network->nodes);
        printf("\r%ld%% ", (long)(100 * progress));
        for(int j = 0; j < 100; j++){
            if(j <= (long)(100 * progress)){
                printf("|");
            }else{
                printf(".");
            }
        }

        struct node* start_node = array_get(network->nodes, i);

        for (int j = 0; j < array_len(start_node->open_edges); j++) {

            // set group size randomly (lager than group_size_minimum_limit)
            int group_size;
            do {
                group_size = group_size_mean + (int) (gsl_ran_ugaussian(random_generator) * group_size_variance);
            } while (group_size < group_size_minimum_limit);

            // init group struct
            struct group *group = malloc(sizeof(struct group));
            group->id = group_seq++;
            group->member = array_initialize(group_size);
            group->member = array_insert(group->member, start_node);

            // initialize prev node stock
            struct edge *edge_from_start_node = array_get(start_node->open_edges, j);
            struct node *current_node = array_get(network->nodes, edge_from_start_node->to_node_id);

            // join nodes to group
            for (int k = 0; k < group_size; k++) {

                // add prev node as group member
                group->member = array_insert(group->member, current_node);

                // if nodes have no edge, end group
                if(array_len(current_node->open_edges) == 0) break;

                // if current_node don't have alternative edge, end group
                int duplicated_count = 0;
                for(int l = 0; l < array_len(current_node->open_edges); l++) {
                    struct edge* edge = array_get(current_node->open_edges, l);
                    for(int m = 0; m < array_len(group->member); m++){
                        struct node* group_member_node = array_get(group->member, m);
                        if(edge->to_node_id == group_member_node->id){
                            duplicated_count++;
                        }
                    }
                }
                if(duplicated_count == array_len(current_node->open_edges)) break;

                // select next node randomly
                struct node *next_node;
                char duplicated;
                do {
                    duplicated = 0;
                    struct edge *edge = array_get(current_node->open_edges, (int)gsl_ran_flat(random_generator, 0, (double)array_len(current_node->open_edges)));
                    next_node = array_get(network->nodes, edge->to_node_id);

                    // if next_node already exists in group
                    for(int l = 0; l < array_len(group->member); l++){
                        struct node *n = array_get(group->member, l);
                        if(next_node->id == n->id){
                            duplicated = 1;
                        }
                    }

                }while(duplicated);

                current_node = next_node;
            }

            // apply group to network
            if(array_len(group->member) >= group_size_minimum_limit) {
                network->groups = array_insert(network->groups, group);
            }
        }
    }
    printf("\n");
}

void construct_groups_close_capacity(struct network *network, gsl_rng *random_generator) {

    // group construction policy
    int group_size_mean = 10;
    int group_size_variance = 5;
    int group_size_minimum_limit = 5;
    float abort_threshold = 0.8f;

    network->groups = array_initialize(1000);
    long group_seq = 0L;

    for (long i = 0; i < array_len(network->nodes); i++) {

        // show progress
        float progress = (float)i / (float)array_len(network->nodes);
        printf("\r%ld%% ", (long)(100 * progress));
        for(int j = 0; j < 100; j++){
            if(j <= (long)(100 * progress)){
                printf("|");
            }else{
                printf(".");
            }
        }

        struct node* start_node = array_get(network->nodes, i);

        for (int j = 0; j < array_len(start_node->open_edges); j++) {

            // set group size randomly (lager than group_size_minimum_limit)
            int group_size;
            do {
                group_size = group_size_mean + (int) (gsl_ran_ugaussian(random_generator) * group_size_variance);
            } while (group_size < group_size_minimum_limit);

            // init group struct
            struct group *group = malloc(sizeof(struct group));
            group->id = group_seq++;
            group->member = array_initialize(group_size);
            group->member = array_insert(group->member, start_node);

            // initialize prev node stock
            struct edge *edge_from_start_node = array_get(start_node->open_edges, j);
            struct node *current_node = array_get(network->nodes, edge_from_start_node->to_node_id);

            // join nodes to group
            for (int k = 0; array_len(group->member) <= group_size; k++) {

                // add prev node as group member
                group->member = array_insert(group->member, current_node);

                // if nodes have no edge, end group
                if (array_len(current_node->open_edges) == 0) break;

                // if current_node don't have alternative edge, end group
                int duplicated_count = 0;
                for (int l = 0; l < array_len(current_node->open_edges); l++) {
                    struct edge *edge = array_get(current_node->open_edges, l);
                    for (int m = 0; m < array_len(group->member); m++) {
                        struct node *group_member_node = array_get(group->member, m);
                        if (edge->to_node_id == group_member_node->id) {
                            duplicated_count++;
                        }
                    }
                }
                if (duplicated_count == array_len(current_node->open_edges)) break;

                // select next node whose capacity is closest to inbound edge
                struct node *prev_node = array_get(group->member, array_len(group->member) - 2);
                struct edge *prev_edge = get_edge_of(prev_node, current_node);
                long min_sq_diff = INT64_MAX;
                struct edge *closest_edge;
                for (int l = 0; l < array_len(current_node->open_edges); l++) {
                    struct edge *edge = array_get(current_node->open_edges, l);

                    // if next_node already exists in group
                    int duplicated = 0;
                    for (int m = 0; m < array_len(group->member); m++) {
                        struct node *n = array_get(group->member, m);
                        if (edge->to_node_id == n->id) {
                            duplicated = 1;
                        }
                    }
                    if (duplicated) continue;

                    // select minimum square difference edge
                    long sq_diff = (long) powl(edge->balance - prev_edge->balance, 2);
                    if (sq_diff < min_sq_diff) {
                        min_sq_diff = sq_diff;
                        closest_edge = edge;
                    }
                }

                if((float)closest_edge->balance / (float)prev_edge->balance < (1.0f-abort_threshold) || (float)closest_edge->balance / (float)prev_edge->balance > (1.0f+abort_threshold)) {
                    break;
                }

                current_node = array_get(network->nodes, closest_edge->to_node_id);
            }

            // apply group to network
            if (array_len(group->member) >= group_size_minimum_limit) {
                network->groups = array_insert(network->groups, group);
/*
                printf("%ld,", group->id);
                long max_edge_balance = 0;
                for(int l =0; l< array_len(group->member); l++){
                    struct node *member_node = array_get(group->member, l);
                    printf("%ld", member_node->id);
                    if(l + 1 < array_len(group->member)){
                        struct edge* e = get_edge_of(array_get(group->member, l), array_get(group->member, l + 1));
                        printf("-(%ld)-", (long)e->balance);
                        if(max_edge_balance < (long)e->balance){
                            max_edge_balance = (long)e->balance;
                        }
                    }else{
                        printf(",");
                    }
                }
                long capacity = calc_group_capacity(group);
                printf("%ld,%ld,%f\n", max_edge_balance, capacity, (float) capacity / (float) max_edge_balance);*/
            }
        }
    }
    printf("\n");
}

long calc_group_capacity(struct group *group) {
    long min = INT64_MAX;
    for (int i = 0; i < array_len(group->member); i++) {
        // if next node exists
        if (i + 1 <= array_len(group->member) - 1) {
            struct node *from_node = array_get(group->member, i);
            struct node *to_node = array_get(group->member, i + 1);
            struct edge *edge = get_edge_of(from_node, to_node);

            if (edge->balance < min) {
                min = (long) edge->balance;
            }
        }
    }
    return min;
}

struct edge *get_edge_of(struct node *from_node, struct node *to_node) {
    for (int i = 0; i < array_len(from_node->open_edges); i++) {
        struct edge *edge = array_get(from_node->open_edges, i);
        if (edge->to_node_id == to_node->id) {
            return edge;
        }
    }
    return NULL;
}

void construct_loop_group(struct network *network) {

    // group construction policy
    int group_size_minimum_limit = 5;
    int group_size_maximum_limit = 10;

    network->groups = array_initialize(1000);
    long group_seq = 0L;

    for (long i = 0; i < array_len(network->nodes); i++) {

        // show progress
        float progress = (float)i / (float)array_len(network->nodes);
        printf("\r%ld%% ", (long)(100 * progress));
        for(int j = 0; j < 100; j++){
            if(j <= (long)(100 * progress)){
                printf("|");
            }else{
                printf(".");
            }
        }

        struct group* group = malloc(sizeof (struct group));
        group->id = group_seq++;
        group->member = array_initialize(100);

        struct node* current_node = array_get(network->nodes, i);

        // while until returned
        search_loop(group, current_node, network, group_size_minimum_limit, group_size_maximum_limit, &group_seq);
    }
    printf("\n");
}

void search_loop(struct group* group, struct node* current_node, struct network* network, int group_size_minimum_limit, int group_size_maximum_limit, long* group_seq) {

    group->member = array_insert(group->member, current_node);

    for(int i = 0; i < array_len(current_node->open_edges); i++){

        struct edge* edge = array_get(current_node->open_edges, i);
        struct node* neighbor_node = array_get(network->nodes, edge->to_node_id);
        struct edge* edge_from_current_node = get_edge_of(current_node, neighbor_node);
        if(edge->id == edge_from_current_node->id) continue;

        struct group* copy = malloc(sizeof (struct group));
        copy->id = (*group_seq)++;
        copy->member = array_initialize(100);
        for(int j = 0; j < array_len(group->member); j++){
            copy->member = array_insert(copy->member, array_get(group->member, j));
        }

        struct node* start_node = array_get(copy->member, 0);

        printf("%ld,%ld,\n", start_node->id, neighbor_node->id);

        if(current_node->id != start_node->id || array_len(copy->member) == 1){
            if(array_len(copy->member) <= group_size_maximum_limit) {
                search_loop(copy, neighbor_node, network, group_size_minimum_limit, group_size_maximum_limit, group_seq);
            }
        } else {
            if(array_len(copy->member) >= group_size_minimum_limit) {
                network->groups = array_insert(network->groups, copy);

                for(int k = 0; k < array_len(copy->member); k++){
                    printf("%ld-", ((struct node*)(array_get(copy->member, k)))->id);
                }
                puts("");
            }
        }
    }
}
