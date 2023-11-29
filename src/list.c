#include <stdio.h>
#include <stdlib.h>
#include "../include/list.h"

struct element* push(struct element* head, void* data) {
	struct element* newhead;

	newhead = malloc(sizeof(struct element));
	newhead->data = data;
  newhead->next = head;

	return newhead;
}

void* get_by_key(struct element* head, long key, int (*is_key_equal)()){
  struct element* iterator;
  for(iterator=head; iterator!=NULL; iterator=iterator->next)
    if(is_key_equal(key, iterator->data))
      return iterator->data;
  return NULL;
}

struct element* pop(struct element* head, void** data) {
  if(head==NULL) {
    *data = NULL;
    return NULL;
  }
  *data = head->data;
  head = head->next;
  return head;
}

long list_len(struct element* head){
  long len;
  struct element* iterator;

  len=0;
  for(iterator=head; iterator!=NULL; iterator=iterator->next)
    ++len;

  return len;
}

unsigned int is_in_list(struct element* head, void* data, int (*is_equal)()){
  struct element* iterator;
  for(iterator=head; iterator!=NULL; iterator=iterator->next)
    if(is_equal(iterator->data, data))
      return 1;
  return 0;
}


void list_free(struct element* head){
  struct element* iterator, *next;
  for(iterator=head; iterator!=NULL;){
    next = iterator->next;
    free(iterator);
    iterator = next;
  }
}

struct element* list_delete(struct element* head, struct element** current_iterator, void* delete_target_data, int (*is_equal)(void*, void*)) {
    struct element* current = head;
    struct element* previous = NULL;

    // Iterate through the list to find the target element
    while (current != NULL && !is_equal(current->data, delete_target_data)) {
        previous = current;
        current = current->next;
    }

    // If the target element is found
    if (current != NULL) {
        // If the target element is the head of the list
        if (previous == NULL) {
            head = current->next;
        } else {
            // Update the next pointer of the previous element
            previous->next = current->next;
        }

        // If delete_target is current_iterator, replace current_iterator with next
        if(current == (*current_iterator)){
            *current_iterator = current->next;
        }

        // Free the memory occupied by the target element
        free(current);
    }

    return head;
}