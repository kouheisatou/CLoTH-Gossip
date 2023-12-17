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

int is_present(long element, struct array* long_array) {
  long i, *curr;

  if(long_array==NULL) return 0;

  for(i=0; i<array_len(long_array); i++) {
    curr = array_get(long_array, i);
    if(*curr==element) return 1;
  }

  return 0;
}

void write_group_update(FILE* csv_group_update, struct group_update* group_update){
    char* type_text;
    if(group_update->type == UPDATE){
        type_text = "UPDATE";
    }else if(group_update->type == CONSTRUCT){
        type_text = "CONSTRUCT";
    }else if(group_update->type == CLOSE){
        type_text = "CLOSE";
    }else{
        type_text = "UNKNOWN";
    }
    fprintf(csv_group_update, "%lu,%lu,%s,%lu,%lu,", group_update->time, group_update->group_id, type_text, group_update->triggered_node_id, group_update->group_cap);
    for(struct element* iterator = group_update->balances_of_edge_in_queue_snapshot; iterator != NULL; iterator = iterator->next){
        uint64_t balance = (uint64_t)iterator->data;
        fprintf(csv_group_update, "%lu", balance);
        if(iterator->next != NULL){
            fprintf(csv_group_update, "-");
        }else{
            fprintf(csv_group_update, "\n");
        }
    }
}

void write_channel_update(FILE* csv_channel_update, struct channel_update* channel_update){
    fprintf(csv_channel_update, "%lu,%lu,%lu\n", channel_update->time, channel_update->edge_id, channel_update->htlc_maximum_msat);
}
