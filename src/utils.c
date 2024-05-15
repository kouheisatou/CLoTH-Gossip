#include <stdlib.h>
#include "../include/utils.h"
#include "../include/routing.h"

int is_equal_long(long* a, long* b) {
  return *a==*b;
}

int is_key_equal(struct distance* a, struct distance* b) {
  return a->node == b->node;
}

int is_equal_edge(struct edge* edge1, struct edge* edge2) {
  return edge1->id == edge2->id;
}

int is_present(long element, struct array* long_array) {
  long i, *curr;

  if(long_array==NULL) return 0;

  for(i=0; i<array_len(long_array); i++) {
    curr = array_get(long_array, i);
    if(*curr==element) return 1;
  }

  return 0;
}

void write_attempt_json(struct attempt* attempt, FILE* csv, struct network* network) {
  fprintf(csv, "{\"\"attempts\"\":%d,\"\"is_succeeded\"\":%d,\"\"time\"\":%lu,\"\"error_edge\"\":%lu,\"\"error_type\"\":%d,\"\"route\"\":[", attempt->attempts, attempt->is_succeeded, attempt->time, attempt->error_edge_id, attempt->error_type);
  for (int j = 0; j < array_len(attempt->route); j++) {
    struct edge_snapshot* edge_snapshot = array_get(attempt->route, j);
    struct edge* edge = array_get(network->edges, edge_snapshot->id);
    struct channel* channel = array_get(network->channels, edge->channel_id);
    fprintf(csv,"{\"\"edge_id\"\":%lu,\"\"from_node_id\"\":%lu,\"\"to_node_id\"\":%lu,\"\"sent_amt\"\":%lu,\"\"edge_cap\"\":%lu,\"\"channel_cap\"\":%lu,", edge_snapshot->id, edge->from_node_id, edge->to_node_id, edge_snapshot->sent_amt, edge_snapshot->balance, channel->capacity);
    if(edge_snapshot->is_included_in_group) fprintf(csv,"\"\"group_cap\"\":%lu,", edge_snapshot->group_cap);
    else fprintf(csv,"\"\"group_cap\"\":null,");
    if(edge_snapshot->does_channel_update_exist) fprintf(csv,"\"\"channel_update\"\":%lu}", edge_snapshot->last_channle_update_value);
    else fprintf(csv,"\"\"channel_update\"\":null}");
    if (j != array_len(attempt->route) - 1) fprintf(csv, ",");
  }
  fprintf(csv, "]}");
}

void write_group_update_json(struct group_update* group_update, FILE* csv, struct network* network) {
  struct group* group = array_get(network->groups, group_update->group_id);
  fprintf(csv, "{\"\"time\"\":%lu,\"\"group_cap\"\":%lu,\"\"type\"\":%d,\"\"edge_balances\"\":[", group_update->time, group_update->group_cap, group_update->type);
  for(int i = 0; i < array_len(group->edges); i++) {
    fprintf(csv, "%lu", group_update->edge_balances[i]);
    if(i != array_len(group->edges)) {
      fprintf(csv, ",");
    }
  }
  fprintf(csv, "]}");
}

void write_channel_update_json(struct channel_update* channel_update, FILE* csv) {
  fprintf(csv, "{\"\"fail_time\"\":%lu,\"\"fail_amount\"\":%lu,\"\"success_time\"\":%lu,\"\"success_amount\"\":%lu}", channel_update->fail_time, channel_update->fail_amount, channel_update->success_time, channel_update->success_amount);
}
