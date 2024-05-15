#ifndef UTILS_H
#define UTILS_H
#include "network.h"
#include "array.h"
#include "routing.h"
#include "htlc.h"

int is_equal_result(struct channel_update *a, struct channel_update *b);

int is_equal_key_channel_update(long key, struct channel_update *a);

int is_equal_edge(struct edge* edge1, struct edge* edge2);

int is_equal_long(long* a, long* b);

int is_present(long element, struct array* long_array);

int is_key_equal(struct distance* a, struct distance* b);

void write_attempt_json(struct attempt* attempt, FILE* csv, struct network* network);

void write_group_update_json(struct group_update* group_update, FILE* csv, struct network* network);

void write_channel_update_json(struct channel_update* channel_update, FILE* csv);

#endif
