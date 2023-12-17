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

void write_group_update(FILE* csv_group_update, struct group_update* group_update);

void write_channel_update(FILE* csv_channel_update, struct channel_update* channel_update);

#endif
