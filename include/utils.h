#ifndef UTILS_H
#define UTILS_H
#include "network.h"
#include "array.h"
#include "routing.h"
#include "htlc.h"

int is_equal_result(struct node_pair_result *a, struct node_pair_result *b);

int is_equal_key_result(long key, struct node_pair_result *a);

int is_equal_edge(struct edge* edge1, struct edge* edge2);

long get_edge_balance(struct edge* e);

int is_equal_group(struct group* group1, struct group* group2);

int is_equal_long(long* a, long* b);

int is_present(long element, struct array* long_array);

int is_key_equal(struct distance* a, struct distance* b);

int can_join_group(struct group* join_target_group, struct edge* joining_edge);

#endif
