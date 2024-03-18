/**
 * @file tos_mem.c
 * @brief memory management
 *
 */

#include "tos_mem.h"
#include "tos_utils.h"
#include <string.h>

/**
 * like the memory management in SGI STL
 * free momory are organized in:
 *     a table of block list, with size of the 8/16/24/32/.../128
 *     a large free area
 * when alloc
 *     find a suitable block in the table
 *     alloc some blocks in the free area
 *     devide one larger block in the table to some smaller blocks
 *
 * When use tos_malloc & tos_free, the block is like: size(4B) + user_space(user_size).
 *   to make addr align, use uint32_t to store size, but real size will not over 256.
 *   so, use hign 16 bits of size as a magic flag.
 */


#define TOS_ADDR_ALIGN         4u
#define TOS_MEM_INITED_FLAG    0x10241024u
#define TOS_MEM_BLOCK_MIN      8u                                        // min block size
#define TOS_MEM_BLOCK_MAX      128u                                      // max block size
#define TOS_MEM_BLOCK_LIST_NUM (TOS_MEM_BLOCK_MAX / TOS_MEM_BLOCK_MIN)   // block list num

#if 1   // if tos_size_t is uint32_t
#define BLOCK_MAGIC        0x2048
#define blk_set_size(size) ((BLOCK_MAGIC << 16) | size)
#define blk_get_size(size) (0xFFFF & size)
#define blk_chk_size(size) ((size >> 16) == BLOCK_MAGIC)
#else
#define blk_set_size(size) size
#define blk_get_size(size) size
#define blk_chk_size(size) true
#endif


typedef union tos_memblk_t {
    union tos_memblk_t* free_list_link;
    uint8_t             raw_space[TOS_MEM_BLOCK_MIN];
    struct {
        tos_size_t size;
        uint8_t    user_space[TOS_MEM_BLOCK_MIN - sizeof(tos_size_t)];
    };
} tos_memblk_t;

typedef struct {
    struct   // cfg
    {
        uint32_t start;
        uint32_t end;
    } mem_pool;
    tos_memblk_t* free_list[TOS_MEM_BLOCK_LIST_NUM];
    uint32_t      init_flag;
} tos_mem_t;

#define TOS_FREELIST_INDEX(bytes) (((bytes) + TOS_MEM_BLOCK_MIN - 1) / TOS_MEM_BLOCK_MIN - 1)
#define TOS_SIZE_ROUND_UP(bytes)  (((bytes) + TOS_MEM_BLOCK_MIN - 1) & ~(TOS_MEM_BLOCK_MIN - 1))


static void*    tos_mem_alloc_new(tos_size_t nbytes);
static uint8_t* tos_mem_chunk_alloc(tos_size_t size, uint8_t* nblks);


static tos_mem_t tos_mem;


/**
 * @brief
 *
 * @param mem_start
 * @param mem_size
 * @return int
 * @note 4 byte align, the space size must be divided by min block size
 */
int tos_mem_module_init(uint32_t mem_start, tos_size_t mem_size) {
    if ((mem_start % TOS_ADDR_ALIGN != 0) || (mem_size % TOS_MEM_BLOCK_MIN != 0)) {
        return -1;
    }

    tos_mem.mem_pool.start = mem_start;
    tos_mem.mem_pool.end   = mem_start + mem_size;
    tos_mem.init_flag      = TOS_MEM_INITED_FLAG;

    memset(tos_mem.free_list, 0, sizeof(tos_mem.free_list));

    return 0;
}


/**
 * @brief
 *
 * @param nbytes 0 < nbytes <= 128
 * @return void* nullptr when fail
 */
void* tos_mem_alloc(tos_size_t nbytes) {
    if (tos_mem.init_flag != TOS_MEM_INITED_FLAG) {
        return nullptr;
    }
    if (nbytes == 0 || nbytes > 128) {
        return nullptr;
    }

    tos_memblk_t** free_list_Nbytes;
    tos_memblk_t*  result;

    free_list_Nbytes = tos_mem.free_list + TOS_FREELIST_INDEX(nbytes);
    result           = *free_list_Nbytes;

    if (result == nullptr)   // not find free block, alloc some new blocks
    {
        return tos_mem_alloc_new(TOS_SIZE_ROUND_UP(nbytes));
    }

    *free_list_Nbytes = result->free_list_link;

    return result;
}


/**
 * @brief
 *
 * @param ptr
 * @param size
 */
void tos_mem_dealloc(void* ptr, tos_size_t size) {
    if (ptr == nullptr || size > TOS_MEM_BLOCK_MAX) {
        return;
    }

    ((tos_memblk_t*)ptr)->free_list_link        = tos_mem.free_list[TOS_FREELIST_INDEX(size)];
    tos_mem.free_list[TOS_FREELIST_INDEX(size)] = (tos_memblk_t*)ptr;
}


/**
 * @brief
 *
 * @param nbytes
 * @return void*
 */
void* tos_malloc(tos_size_t nbytes) {
    uint8_t* mem = tos_mem_alloc(nbytes + sizeof(tos_size_t));

    if (mem == nullptr) {
        return nullptr;
    }

    *(tos_size_t*)mem = blk_set_size(nbytes);

    return mem + sizeof(tos_size_t);
}

/**
 * @brief
 *
 * @param ptr
 */
void tos_free(void* ptr) {
    if (ptr == nullptr) {
        return;
    }

    uint8_t*   mem    = (uint8_t*)ptr - sizeof(tos_size_t);
    tos_size_t nbytes = *(tos_size_t*)mem;

    if (blk_chk_size(nbytes)) {
        nbytes = blk_get_size(nbytes);

        tos_mem_dealloc(mem, nbytes + sizeof(tos_size_t));
    } else {
        tos_error("try to free an invalid block\n");
    }
}


/**
 * @brief try to alloc some nbytes block
 *
 * @param nbytes
 * @return void*
 */
static void* tos_mem_alloc_new(tos_size_t nbytes) {
    void*    result;
    uint8_t  nblks = 5;
    uint8_t* chunk = tos_mem_chunk_alloc(nbytes, &nblks);   // alloc nblks block of nbytes

    if (nblks == 0) {
        return nullptr;
    } else if (nblks == 1) {
        return chunk;
    }

    result = chunk;

    /* Build free list in chunk */
    tos_memblk_t *current_block, *next_block;

    next_block = tos_mem.free_list[TOS_FREELIST_INDEX(nbytes)] = (tos_memblk_t*)(chunk + nbytes);

    // add (nblks-1) blocks to free list
    for (uint8_t i = 1; i <= nblks - 1; i++) {
        current_block = next_block;
        next_block    = (tos_memblk_t*)((uint8_t*)next_block + nbytes);

        current_block->free_list_link = next_block;
    }

    current_block->free_list_link = nullptr;

    return result;
}


/**
 * @brief try to alloc nblks blocks with size
 *
 * @param size
 * @param nblks
 * @return uint8_t*
 */
static uint8_t* tos_mem_chunk_alloc(tos_size_t size, uint8_t* nblks) {
    uint8_t* result;
    uint32_t total_bytes = size * (*nblks);
    uint32_t bytes_left  = tos_mem.mem_pool.end - tos_mem.mem_pool.start;

    if (bytes_left >= total_bytes) {
        result = (uint8_t*)tos_mem.mem_pool.start;
        tos_mem.mem_pool.start += total_bytes;

        return result;
    } else if (bytes_left >= size) {
        *nblks      = bytes_left / size;   // real block num
        total_bytes = size * (*nblks);

        result = (uint8_t*)tos_mem.mem_pool.start;
        tos_mem.mem_pool.start += total_bytes;

        return result;
    } else {
        // make use of the left-over piece. the left piece must can be add in one block list
        if (bytes_left > 0) {
            ((tos_memblk_t*)tos_mem.mem_pool.start)->free_list_link = tos_mem.free_list[TOS_FREELIST_INDEX(bytes_left)];
            tos_mem.free_list[TOS_FREELIST_INDEX(bytes_left)]       = (tos_memblk_t*)tos_mem.mem_pool.start;
        }

        // free a larger free block
        uint8_t blksize;
        for (blksize = size; blksize <= TOS_MEM_BLOCK_MAX; blksize += TOS_MEM_BLOCK_MIN) {
            if (tos_mem.free_list[TOS_FREELIST_INDEX(blksize)] != nullptr) {
                tos_mem.free_list[TOS_FREELIST_INDEX(blksize)] =
                    tos_mem.free_list[TOS_FREELIST_INDEX(blksize)]->free_list_link;
                tos_mem.mem_pool.start = (uint32_t)tos_mem.free_list[TOS_FREELIST_INDEX(blksize)];
                tos_mem.mem_pool.end   = tos_mem.mem_pool.start + blksize;

                return tos_mem_chunk_alloc(size, nblks);
            }
        }

        return nullptr;
    }
}
