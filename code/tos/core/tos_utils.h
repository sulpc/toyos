/**
 * @file tos_utils.h
 * @brief base macro and utils used by tos
 */

#ifndef _TOS_UTILS_H_
#define _TOS_UTILS_H_


#define tos_error(...)                                                                                                 \
    do {                                                                                                               \
        /*__VA_ARGS__*/                                                                                                \
    } while (0)

// Init queue to empty
#define tos_queue_init(node_ptr)                                                                                       \
    do {                                                                                                               \
        (node_ptr)->prev = (node_ptr);                                                                                 \
        (node_ptr)->next = (node_ptr);                                                                                 \
    } while (0)

// Insert new node to tail of queue (front of head node)
#define tos_queue_insert(queue, new_node)                                                                              \
    do {                                                                                                               \
        (new_node)->prev    = (queue)->prev;                                                                           \
        (new_node)->next    = (queue);                                                                                 \
        (queue)->prev->next = (new_node);                                                                              \
        (queue)->prev       = (new_node);                                                                              \
    } while (0)

// Remove node from queue. No effect if node is not in list
#define tos_queue_remove(node_ptr)                                                                                     \
    do {                                                                                                               \
        (node_ptr)->prev->next = (node_ptr)->next;                                                                     \
        (node_ptr)->next->prev = (node_ptr)->prev;                                                                     \
    } while (0)

#define tos_queue_is_empty(node_ptr) (((node_ptr)->prev == (node_ptr)) && ((node_ptr)->next == (node_ptr)))

#define get_object_by_field(type, filedname, filedptr)                                                                 \
    ((type*)((uint8_t*)(filedptr) - (uint32_t) & ((type*)0)->filedname))


// Queue Element
typedef struct tos_queue_node_t tos_queue_node_t;

struct tos_queue_node_t {
    tos_queue_node_t* prev;
    tos_queue_node_t* next;
};


#endif
