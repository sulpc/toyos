#include "util_fifo_buffer.h"
#include <string.h>


/**
 * @brief
 *
 * @param fifo
 * @param data
 * @param data_size number of bytes want get
 * @return uint16_t number of bytes really get
 */
uint16_t fifo_buffer_get_block(fifo_buffer_t* fifo, uint8_t* data, uint16_t data_size) {
#if 1
    uint16_t copyed_size = 0;
    while (!fifo_buffer_empty(fifo) && copyed_size < data_size) {
        fifo_buffer_get(fifo, &data[copyed_size]);
        copyed_size++;
    }
    return copyed_size;
#else
    if (fifo->pos_rd == fifo->pos_wr) {
        return 0;
    } else if (fifo->pos_rd < fifo->pos_wr) {
        // -----rd++++++++++wr-----  +: data could get
        if (data_size < fifo->pos_wr - fifo->pos_rd) {
            memcpy(data, &fifo->buffer[fifo->pos_rd], data_size);
            return data_size;
        } else {
            memcpy(data, &fifo->buffer[fifo->pos_rd], fifo->pos_wr - fifo->pos_rd);
            return fifo->pos_wr - fifo->pos_rd;
        }
    } else {
        // +++++wr----------rd+++++  +: data could get
        if (data_size < fifo->buffer_size - fifo->pos_rd) {
            memcpy(data, &fifo->buffer[fifo->pos_rd], data_size);
            return data_size;
        } else {
            uint16_t copyed_size = fifo->buffer_size - fifo->pos_rd;
            memcpy(data, &fifo->buffer[fifo->pos_rd], copyed_size);

            if (data_size - copyed_size < fifo->pos_wr) {
                memcpy(data + copyed_size, &fifo->buffer[0], data_size - copyed_size);
                return data_size;
            } else {
                memcpy(data + copyed_size, &fifo->buffer[0], fifo->pos_wr);
                return fifo->pos_wr + copyed_size;
            }
        }
    }
#endif
}


/**
 * @brief
 *
 * @param fifo
 * @param data
 * @param data_size number of bytes want put
 * @return uint16_t number of bytes really put
 */
uint16_t fifo_buffer_put_block(fifo_buffer_t* fifo, uint8_t* data, uint16_t data_size) {
#if 1
    uint16_t copyed_size = 0;
    while (!fifo_buffer_full(fifo) && copyed_size < data_size) {
        fifo_buffer_put(fifo, data[copyed_size]);
        copyed_size++;
    }
    return copyed_size;
#else
    // TODO
#endif
}
