#ifndef list_h
#define list_h


struct element {
	struct element* next;
	struct element* prev;
	void* data;
};

struct element* push(struct element* head, void* data);

void* get_by_key(struct element* head, long key, int (*is_key_equal)());

struct element* pop(struct element* head, void** data);

long list_len(struct element* head);

unsigned int is_in_list(struct element* head, void* data, int(*is_equal)());

void list_free(struct element* head);

struct element* list_delete(struct element* head, struct element** current_iterator, void* delete_target_data, int (*is_equal)(void*, void*));

struct element* list_insert_after(struct element* insert_position, void* data, struct element* head, struct element** inserted_data);

struct element* list_insert_sorted_position(struct element* head, void* data, long (*get_sort_target_value)(void*), struct element** inserted_data);

#endif
