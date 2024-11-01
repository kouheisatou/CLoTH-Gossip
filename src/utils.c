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

long get_edge_balance(struct edge* e){
    return e->balance;
}

int is_equal_group(struct group* group1, struct group* group2) {
  return group1->id == group2->id;
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

int can_join_group(struct group *join_target_group, struct edge *joining_edge) {

    // check join_target_group conditions
    if (joining_edge->balance < join_target_group->min_cap_limit ||
        joining_edge->balance > join_target_group->max_cap_limit) {
        return 0;
    }

    // check groups doesn't contain connected 2 links
    for (int i = 0; i < array_len(join_target_group->edges); i++) {
        struct edge *e = array_get(join_target_group->edges, i);
        if (joining_edge == e) return 0;
        if (joining_edge->to_node_id == e->to_node_id ||
            joining_edge->to_node_id == e->from_node_id ||
            joining_edge->from_node_id == e->to_node_id ||
            joining_edge->from_node_id == e->from_node_id) {
            return 0;
        }
    }

    // check same pair doesn't exist in existing join_target_group
    for (struct element *iterator = joining_edge->groups; iterator != NULL; iterator = iterator->next) {
        struct group *joining_edges_group = iterator->data;
        for (int i = 0; i < array_len(joining_edges_group->edges); i++) {
            struct edge *edge_in_joining_edges_group = array_get(joining_edges_group->edges, i);
            for (int j = 0; j < array_len(join_target_group->edges); j++) {
                struct edge *edge_in_join_target_group = array_get(join_target_group->edges, j);
                if (edge_in_joining_edges_group->id == edge_in_join_target_group->id) {
                    return 0;
                }
            }
        }
    }

    return 1;
}