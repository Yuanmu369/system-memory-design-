#ifndef MY_MEMORY_H
#define MY_MEMORY_H

#include "interface.h"

// Declare your own data structures and functions here...

#define MAX_ORDER 15  // 8M空间可以分为15阶, 最小块是 512 字节

/* 注意：
 * 内部地址计算均使用整型，在外部统一再转换
 */

// 每个头信息
struct chunk_info {
    int in_use; // 是否在使用
    int size;   // 当前大小
    //int power;  // 幂次方
    int start;  // 地址起始位置
    int end;    // 地址结束位置
    struct chunk_info *prev;    // 前一个
    struct chunk_info *next;    // 下一个位置
};

// 按阶分区
struct free_area {
    struct chunk_info *info;
    int chunk_size;
    int power_size;
    int num_free;
};

struct slab_info {
    int num_free;       // 还可使用的数
    uint64_t bits;      // bit 位，标记已使用的位置
    int size;           // 当前 slab 每个单位占的大小
    int mem_pos;        // 记录该板所在内存的对应位置

    // 单链, 所使用的链表, 保存不同大小板
    struct slab_info *next;
};

// 测试函数
void print_areas(const char *tip);

// 初始化 areas
// 初始化 areas, 只有最大的一个
// 其它的都初始化为空
void buddy_init();

// 申请内存
// 先找到最合适的大小，然后在 areas 链表中查找
// 找到了，则进行标记
// 没有找到，则找到大一阶的，然后进行分割，从 areas 中相应的阶中删除此记录，并在低一阶中记录分开的两个记录
int buddy_malloc(int size, void *start_of_memory);

// 释放内存
// 先根据 size 头计算出当前的大小找到对应的位置
// 判断前后是否也为空闲，是则进行合并，将删除原有的小记录，保存到大记录中
void buddy_free(int mem_pos, void *start_of_memory);

// slab 策略初始化
void slab_init();

// slab 内存申请
int slab_malloc(int size, void *start_of_memory);

// slab 内存释放
void slab_free(int mem_pos, void *start_of_memory);

#endif
