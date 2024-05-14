#ifndef UTILS_H
#define UTILS_H
#include "network.h"
#include "array.h"
#include "routing.h"
#include "htlc.h"

int is_equal_result(struct node_pair_result *a, struct node_pair_result *b);

int is_equal_key_result(long key, struct node_pair_result *a);

int is_equal_long(long* a, long* b);

int is_present(long element, struct array* long_array);

int is_key_equal(struct distance* a, struct distance* b);

void write_attempt_json(struct attempt* attempt, FILE* csv, struct network* network);

void write_group_update_json(struct group_update* group_update, FILE* csv, struct network* network);

#endif
