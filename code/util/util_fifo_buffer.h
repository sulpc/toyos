#ifndef _UTIL_FIFO_BUFFER_H_
#define _UTIL_FIFO_BUFFER_H_


#include "util_types.h"


typedef struct {
    uint8_t* buffer;
    uint16_t buffer_size;
    uint16_t pos_rd;
    uint16_t pos_wr;
} fifo_buffer_t;


#define fifo_buffer_init(fifo, buf, size)                                                                              \
    do {                                                                                                               \
        (fifo)->buffer      = (buf);                                                                                   \
        (fifo)->buffer_size = (size);                                                                                  \
        (fifo)->pos_rd      = 0;                                                                                       \
        (fifo)->pos_wr      = 0;                                                                                       \
    } while (0)
#define fifo_buffer_empty(fifo) ((fifo)->pos_rd == (fifo)->pos_wr)
#define fifo_buffer_full(fifo)  ((((fifo)->pos_wr + 1) % (fifo)->buffer_size) == (fifo)->pos_rd)
#define fifo_buffer_get(fifo, data)                                                                                    \
    do {                                                                                                               \
        *(data) = (fifo)->buffer[(fifo)->pos_rd];                                                                      \
        (fifo)->pos_rd += 1;                                                                                           \
        if ((fifo)->pos_rd == (fifo)->buffer_size) {                                                                   \
            (fifo)->pos_rd = 0;                                                                                        \
        }                                                                                                              \
    } while (0)
#define fifo_buffer_put(fifo, data)                                                                                    \
    do {                                                                                                               \
        (fifo)->buffer[(fifo)->pos_wr] = data;                                                                         \
        (fifo)->pos_wr += 1;                                                                                           \
        if ((fifo)->pos_wr == (fifo)->buffer_size) {                                                                   \
            (fifo)->pos_wr = 0;                                                                                        \
        }                                                                                                              \
    } while (0)


uint16_t fifo_buffer_get_block(fifo_buffer_t* fifo, uint8_t* data, uint16_t data_size);
uint16_t fifo_buffer_put_block(fifo_buffer_t* fifo, uint8_t* data, uint16_t data_size);


#endif
