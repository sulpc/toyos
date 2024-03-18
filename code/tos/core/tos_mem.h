/**
 * @file tos_mem.h
 * @brief memory management
 *
 */

#ifndef _TOS_MEM_H_
#define _TOS_MEM_H_


#include "tos_types.h"


/**
 * @brief
 *
 * @param mem_start
 * @param mem_size
 * @return int
 * @note 4 byte align, the space size must be divided by min block size
 */
int tos_mem_module_init(uint32_t mem_start, tos_size_t mem_size);

/**
 * @brief
 *
 * @param nbytes 0 < nbytes <= 128
 * @return void* nullptr when fail
 */
void* tos_mem_alloc(tos_size_t nbytes);

/**
 * @brief
 *
 * @param ptr
 * @param size
 */
void tos_mem_dealloc(void* ptr, tos_size_t size);

/**
 * @brief
 *
 * @param nbytes
 * @return void*
 */
void* tos_malloc(tos_size_t nbytes);

/**
 * @brief
 *
 * @param ptr
 */
void tos_free(void* ptr);


#endif
