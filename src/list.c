#include <stdio.h>
#include <stdlib.h>
#include "../include/list.h"

struct element* push(struct element* head, void* data) {
	struct element* newhead;

	newhead = malloc(sizeof(struct element));
	newhead->data = data;
  newhead->next = head;
  if(head != NULL) head->prev = newhead;
  newhead->prev = NULL;

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
  struct element* newhead = head->next;
    free(head);
  if(newhead != NULL) newhead->prev = NULL;
  return newhead;
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

    // Iterate through the list to find the target element
    struct element* delete_target = head;
    while (delete_target != NULL && !is_equal(delete_target->data, delete_target_data)) {
        delete_target = delete_target->next;
    }

    // If the target element is found
    if (delete_target != NULL) {
        // If the target element is the head of the list
        if (delete_target->prev == NULL) {
            head = delete_target->next;
            head->prev = NULL;
        } else {
            // Update the next pointer of the previous element
            delete_target->next->prev = delete_target->prev;
            delete_target->prev->next = delete_target->next;
        }

        // If delete_target is current_iterator, replace current_iterator with next
        if(current_iterator != NULL && delete_target == (*current_iterator)){
            *current_iterator = delete_target->next;
        }

        // Free the memory occupied by the target element
        free(delete_target);
    }

    return head;
}

struct element* list_insert_after(struct element* insert_position, void* data, struct element* head){
    if(insert_position != NULL){
        struct element* new_element = malloc(sizeof(struct element));
        struct element* prev = insert_position;
        struct element* next = insert_position->next;
        new_element->data = data;
        new_element->next = next;
        new_element->prev = prev;
        if(next != NULL) next->prev = new_element;
        prev->next = new_element;
    }else{
        head = push(head, data);
    }

    return head;
}

struct element* list_insert_sorted_position(struct element* head, void* data, long (*get_sort_value)(void*)){
    if(head == NULL) {
        head = push(head, data);
        return head;
    }

    for (struct element* iterator = head; iterator != NULL; iterator = iterator->next){

        // insert last
        if(get_sort_value(iterator->data) < get_sort_value(data) && iterator->next == NULL){
            head = list_insert_after(iterator, data, head);
            break;
        }
        // insert first
        else if(iterator == head && get_sort_value(data) <= get_sort_value(iterator->data)){
            head = push(head, data);
            break;
        }
        // insert middle
        else if(get_sort_value(iterator->data) < get_sort_value(data) && get_sort_value(data) <= get_sort_value(iterator->next->data)){
            head = list_insert_after(iterator, data, head);
            break;
        }
    }

    return head;
}