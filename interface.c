#include "interface.h"
#include "my_memory.h"

// Interface implementation
// Implement APIs here...

// 分别记录内存类型及原始内存首地址
static enum malloc_type m_type;
static int m_size;
static void *s_mem;

void my_setup(enum malloc_type type, int mem_size, void *start_of_memory)
{
    m_type = type;
    s_mem = start_of_memory;
    m_size = mem_size;

    buddy_init();
    // TODO: slab_init();
}

void *my_malloc(int size)
{
    int pos = 0;
    if (m_type == MALLOC_BUDDY) {
        pos = buddy_malloc(size, s_mem);
    } else {
        pos = slab_malloc(size, s_mem);
//        // 跳过 slab 的 HEADER_SIZE
//        pos += HEADER_SIZE;
    }

    //printf("return pos: %d\n", pos + HEADER_SIZE);

    if (pos < 0) {
        // 申请失败，返回空指针
        return NULL;
    }

    // 向后移8个地址为可用的地址
    return (void *)((char *)s_mem + pos + HEADER_SIZE);
}

void my_free(void *ptr)
{
    // 向前移8个地址为要释放的实际地址带头部的
    int pos = (char *)ptr - (char *)s_mem - HEADER_SIZE;

    if (m_type == MALLOC_BUDDY) {
        buddy_free(pos, s_mem);
    } else {
        slab_free(pos, s_mem);
    }
}
