#include <stdlib.h>
#include "../include/utils.h"
#include "../include/routing.h"

int is_equal_result(struct node_pair_result *a, struct node_pair_result *b ){
  return a->to_node_id == b->to_node_id;
}

int is_equal_key_result(long key, struct node_pair_result *a){
  return key == a->to_node_id;
}

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

void write_channel_update(FILE* csv_channel_update, struct channel_update* channel_update){
    fprintf(csv_channel_update, "%lu,%lu,%lu\n", channel_update->time, channel_update->edge_id, channel_update->htlc_maximum_msat);
}
