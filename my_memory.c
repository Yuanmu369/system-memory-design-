#include "my_memory.h"

// Memory allocator implementation
// Implement all other functions here...

static struct free_area areas[MAX_ORDER];

static struct slab_info *slabs;
//
static int my_pow(int num)
{
    int i, ret = 1;
    if (num == 0)
        return 1;

    for (i = 0; i < num; i++)
        ret *= 2;

    return ret;
}

// 计算出幂
int get_power(int size)
{
    int pow = 0;
    // 除上块的长度
    if (size % MIN_MEM_CHUNK_SIZE != 0) {
        // 非整块大小，需要加1
        size /= MIN_MEM_CHUNK_SIZE;
        size++;
    } else {
        // 整块则不用
        size /= MIN_MEM_CHUNK_SIZE;
    }

    // 找一个可以容纳 size 的幂
    // 去下面找内存块
    int i, ret = 1;
    for (i = 0; ;i++) {
        if (ret >= size) {
            return i;
        }

        ret *= 2;
    }

    // 失败，走不到这里的
    return -1;
}

// 初始化 areas
// 初始化 areas, 只有最大的一个
// 其它的都初始化为空
void buddy_init()
{
    // 测试一下 get_power()
    int i;
    for (i = 0; i < MAX_ORDER; i++) {
        areas[i].chunk_size = 512 * my_pow(i);
        areas[i].power_size = i;
        areas[i].num_free = 0;
    }
//
    areas[MAX_ORDER-1].info = (struct chunk_info *)malloc(sizeof(struct chunk_info));
    areas[MAX_ORDER-1].info->in_use = 0;
    areas[MAX_ORDER-1].info->size = MEMORY_SIZE;
    //areas[MAX_ORDER-1].info->power = 13;
    areas[MAX_ORDER-1].info->start = 0;
    areas[MAX_ORDER-1].info->end = MEMORY_SIZE;
    areas[MAX_ORDER-1].num_free = 1;
}

// 从链表中删除一个元素，不负责 free
void remove_from_link(int i, struct chunk_info *p)
{
    //从链表里删除东西，如果是空的话就不用删除
    if (areas[i].info == NULL) {
        // 没有元素就不处理
        //printf("%p not in areas[%d]\n", p, i);
        return;
    }

    //printf("remove_from_link %p\n", p);
    if (areas[i].info == p) {
        // 是第一个元素
        areas[i].info = p->next;
        if (areas[i].info == NULL) {
            // 如果只这一个元素, 删除后就没有了，areas[i].info 指向 NULL
        } else {
            // info 头部已经没有东西了，所以就指空了
            areas[i].info->prev = NULL;
//            if (areas[i].info->next == NULL) {
//                // 删除后还有一个元素, 则自己的 prev 指向 NULL
//                areas[i].info->prev = NULL;
//            } else {
//                // 如果还有多个，则指向前一个
//                areas[i].info->next->prev = areas[i].info;
//            }
        }
    } else {
        // 连接元素 就是把p删除了
        p->prev->next = p->next;

        // 如果 p 是不最后一个元素    把p后面的元素连到他前面的元素，连在一起了

        if (p->next != NULL) {
            p->next->prev = p->prev;
        }
    }
}

// 从链表中添加一个元素
void add_to_link(int i, struct chunk_info *p)
{
    //printf("add_to_link %p\n", p);
    // 加入到头部
    p->prev = NULL;
    if (areas[i].info == NULL) {
        //printf("add to head.\n");
        // 还没有元素
        p->next = NULL;
        areas[i].info = p;
        //printf("info: %p, next: %p\n", p, p->next);
    } else {
        // 插入到头部
        p->next = areas[i].info;
        p->next->prev = p;
        areas[i].info = p;
        //printf("info: %p, next: %p, next->next: %p\n", p, p->next, p->next->next);
    }
}

void print_areas(const char *tip)
{
    printf("***** %s ******\n", tip);
    int i;
    for (i = 0; i < MAX_ORDER; i++) {
        printf("%d: %d\n", i, areas[i].num_free);
        struct chunk_info *p = areas[i].info;
        int i = 0;
        while (p) {
            printf("[%c] %p %d-%d\n", p->in_use ? 'Y' : 'N', p, p->start/512, p->end/512);
            p = p->next;
//            if (++i > 5)
//                break;
        }
        //printf("\n");
    }
}

// 对内存进行分割
// i 要分割的阶数
// TODO: 切分的时候要写入 HEADER_SIZE
static int split_chunk(struct chunk_info *p, int i, void *start_of_memory)
{
    struct chunk_info *buddy = (struct chunk_info *)malloc(sizeof (struct chunk_info));
    struct chunk_info *first = (struct chunk_info *)malloc(sizeof (struct chunk_info));

    // 分割为两个块
    // 先构造 后buddy
    buddy->in_use = 0;
    buddy->size = p->size / 2;
    buddy->start = p->start + p->size / 2;
    buddy->end = p->start + p->size;
//村大小的  加上buddy start 的8个字节
    *(uint64_t *)((char *)start_of_memory + buddy->start) = buddy->size;

    // p 为前 buddy
    first->in_use = 0;
    first->size = p->size / 2;
    first->start = p->start;
    first->end = p->start + p->size / 2;

    *(uint64_t *)((char *)start_of_memory + first->start) = first->size;

    printf("split %d size: %d to: %d\n", i, p->size/512, i-1);

    // 将两个数据加入到低一阶中
    add_to_link(i-1, buddy);
    add_to_link(i-1, first);

    // 低一阶多了两个可用块
    areas[i-1].num_free += 2;

    // 先从当前阶中删除该内存块
    remove_from_link(i, p);

    // p 仍在使用，不要 free
    // 当前阶可用少一个可用块
    areas[i].num_free--;
}

// 申请内存
// 先找到最合适的大小，然后在 areas 链表中查找
// 找到了，则进行标记
// 没有找到，则找到大一阶的，然后进行分割，从 areas 中相应的阶中删除此记录，并在低一阶中记录分开的两个记录
// size 未带上 HEADER_SIZE
// 返回的是带头的具体位置
int buddy_malloc(int size, void *start_of_memory)
{
    printf("malloc size: %d\n", size);
    // 实际要申请的要多加8个字节
    size += HEADER_SIZE;
    // 根据 size 计算出最小的幂
    int i = get_power(size);
    int pow = i;

    // 先在对应的阶中查找
    if (areas[i].num_free > 0) {
        // 有空闲的，则返回并标记相应的区域
        struct chunk_info *p = areas[i].info;
        while (p && p->in_use) {
            // 循环完他所有的元素
            p = p->next;
        }

        if (p == NULL) {
            printf("没有，怎么进来的？\n");
            return -1;
        }

        // 必定能找到
        p->in_use = 1;
        areas[i].num_free--;

        printf("direct found.\n");

        return p->start;
    }

    // 在同阶中没有找到,则向往后的阶中找
    // 然后进行分割
    i++;
    // 这里 i 必然大于 0
    int done = 0;
    for (; i < MAX_ORDER; i++) {
        if (areas[i].num_free > 0) {
            // 如果当前有空闲的，则进行分割
            // 有可使用空间，则到此结束
            break;
        }
    }

    if (i == MAX_ORDER) {
        // 申请不到了
        return -1;
    }

    // 打印刚开始的空间
    //print_areas("init");

    // 开始向下查找分割
    for (; i > pow; i--) {
        // 查找到第一个, 然后进行分割
        struct chunk_info *p = areas[i].info;

        while (p) {
            if (! p->in_use) {
                // 找到了进行分割
                split_chunk(p, i, start_of_memory);
                // 分隔一次后再看看
                //print_areas("split");
                break;
            }
            p = p->next;
        }
    }

    //printf("split end.\n");
// 刚才只是做了分割，现在开始找对应的地址然后返还地址
    // 重新在低阶中查找
    struct chunk_info *p = areas[pow].info;
    while (p && p->in_use) {
        // 如果有在使用，则循环
        p = p->next;
    }

    if (p == NULL) {
        // 没找到？
        printf("坏了，没找到可用空间\n");
        return -1;
    }

    //printf("找到了.\n");

    // 必定能找到
    p->in_use = 1;
    areas[pow].num_free--;

    // 返回可使用地址
    return p->start;
}

// 释放内存
// 判断前后是否也为空闲，是则进行合并，将删除原有的小记录，保存到大记录中
// pos 为实际块的地址，包含 HEADER_SIZE
void buddy_free(int mem_pos, void *start_of_memory)
{
    // 计算首先要释放的所在的阶
    uint64_t size = *(uint64_t *)((char *)start_of_memory + mem_pos);
    int pow = get_power(size);
    printf("first pow: %d\n", pow);

    // 开始向上合并
    int i = pow;
    for (; i < MAX_ORDER - 1; i++) {
        // 先找出该信息
        struct chunk_info *p = areas[i].info;
        while (p) {
            // 根据起始位置查找
            if (p->start == mem_pos) {
                // 找到了该指针的信息
                // 必定找到
                break;
            }
            p = p->next;
        }

        if (! p) {
            // 未找到，有问题
            printf("坏了 [%d] 没找到 mem_pos: %d\n", i, mem_pos/MIN_MEM_CHUNK_SIZE);
            return;
        }

        // 标记当前块为可用状态
        // 当为不可用状态时，才标记为可用，否则会影响后面的合并
        if (p->in_use == 1) {
            p->in_use = 0;
            printf("free %d[%d]\n", p->start/MIN_MEM_CHUNK_SIZE, p->size/MIN_MEM_CHUNK_SIZE);

            // 当前可用数加1
            areas[i].num_free++;
        }

        // 指针计算 buddy
        int buddy_pos = 0;

        // 当前块是首还是尾？
        int begin = 0;
        if (mem_pos % (p->size * 2) == 0) {
            // 是首
            begin = 1;
        }

        if (begin) {
            // p是伙伴前
            buddy_pos = mem_pos + p->size;
        } else {
            // p是伙伴后
            buddy_pos = mem_pos - p->size;
        }

        printf("mem_pos: %d, buddy_pos: %d\n", mem_pos/MIN_MEM_CHUNK_SIZE, buddy_pos/MIN_MEM_CHUNK_SIZE);

        // 查找 buddy 信息
        struct chunk_info *bp = areas[i].info;

        while (bp) {
            if (bp->start == buddy_pos) {
                // 找到了该指针的信息
                // 必定找到
                break;
            }
            bp = bp->next;
        }

        if (! bp) {
            // 未找到，有问题
            printf("坏了 [%d] 没找到 bp: %d\n", i, buddy_pos);
            return;
        }

        // 现在要释放 p 如果它的 buddy 也是空闲的话，就进行合并
        if (bp->in_use) {
            // 仍在使用，则不能合并
            // 后面高阶的也不会有合并
            break;
        }

        // 进行合并
        // 从当前阶中删除这两个
        remove_from_link(i, p);
        remove_from_link(i, bp);
        printf("combine %d %d[%d], %d[%d]\n", i, p->start/MIN_MEM_CHUNK_SIZE,
                p->size/MIN_MEM_CHUNK_SIZE,
                bp->start/MIN_MEM_CHUNK_SIZE,
                bp->size/MIN_MEM_CHUNK_SIZE);

        // 当前阶少了两个可用块
        areas[i].num_free -= 2;
        printf("after combine num_free: %d\n", areas[i].num_free);

        // 在高一阶中加入一个
        struct chunk_info *top = (struct chunk_info *)malloc(sizeof(struct chunk_info));

        // 填充节点数据信息
        top->in_use = 0;
        top->size = p->size * 2;
        top->start = begin ? p->start : bp->start;
        top->end = begin ? bp->end : p->end;

        // 添加到高一阶上
        add_to_link(i+1, top);
        areas[i+1].num_free++;
        printf("after combine +1 num_free: %d\n", areas[i+1].num_free);

        // 删除上面的节点
        free(p);
        free(bp);

        // p 指向下一个可能要释放的位置
        mem_pos = begin ? mem_pos : buddy_pos;
    }
}

// 判断位是否在使用    slab一共64个板，看哪个板再用  这个function做标示
int is_use(uint64_t bits, int pos)
{
    if (bits & ((uint64_t)1<<pos)) {
        return 1;
    }

    return 0;
}

// 对位进行标记为未使用或已使用
void set_bit(struct slab_info *s, int pos, int type)
{
    
    if (type == 0) {
        // 标记为未使用
        s->bits &= ~((uint64_t)1<<pos);
    } else {
        // 标记为已使用
        s->bits |= (uint64_t)1<<pos;
    }
}

// slab 策略初始化
void slab_init()
{
    // 初始化 buddy
    // 注意：buddy_init 不能初始化二次
    buddy_init();
}

// slab 内存申请
// size 为单个的大小，但我们得给申请多个
// 返回的 pos 带有 HEADER_SIZE
int slab_malloc(int size, void *start_of_memory)
{
    struct slab_info *p = slabs;

    while (p) {
        // 存在多个块，前面的块可能已经使用完了   一个slab最多64个，比如你申请了80个，你就要申请第二个slab
        if (p->size == size && p->num_free > 0) {
            // 找到了对应 size 的板
            break;
        }
        p = p->next;
    }
//找到了！
    if (p) {
        // 看这个板中是否有可用空间
        if (p->num_free > 0) {
            // 找一个空间位置
            int i;
            for (i = 0; i < N_OBJS_PER_SLAB; i++) {
                //开始找到底哪个可以用
                if (! is_use(p->bits, i)) {
                    p->num_free--;
                    set_bit(p, i, 1);// 设置成1
                    // 找到了，返回内存位置
                    // slab 开始位置 + i * (p->size + HEADER_SIZE)
                    return p->mem_pos + i * (p->size + HEADER_SIZE);
                }
            }
        }
    }

    // 没有对应的空间，则再申请一个 slab
    // 所需的整个大小 每个板都有个头，再乘以板中对象数 N_OBJS_PER_SLAB
    int all_size = (size + HEADER_SIZE) * N_OBJS_PER_SLAB;

    // 使用 buddy_malloc 申请一个空间
    // mem_pos 得跳过 buddy 的 HEADER_SIZE
    // mem_pos 是 slab 可使用的真正空间
    //要使用buddy malloc来创建一个这么大的空间
    int mem_pos = buddy_malloc(all_size, start_of_memory) + HEADER_SIZE;
    printf("malloc from buddy, size: %d, all_size: %d, pos: %d\n", size, all_size, mem_pos);
// 太大了！
    if (mem_pos - HEADER_SIZE < 0) {
        // 申请失败返回 -1
        return -1;
    }

    struct slab_info *s = (struct slab_info *)malloc(sizeof(struct slab_info));
    // 填充信息
    s->num_free = N_OBJS_PER_SLAB;
    s->bits = 0;
    s->size = size;
    s->mem_pos = mem_pos;
    s->next = NULL;

    // 为申请的对象内存空间的头部保存大小
    // 用于内存的释放
    //每个板都有一个大小，是用来锁定位置的
    int i;
    for (i = 0; i < N_OBJS_PER_SLAB; i++) {
        *((uint64_t *)((char *)start_of_memory + s->mem_pos + i * (HEADER_SIZE + size))) = size;
    }

    // 新的插入到尾部
    struct slab_info *tmp = slabs;
    if (tmp == NULL) {
        slabs = s;
    } else {
        while (tmp && tmp->next != NULL) {
            tmp = tmp->next;
        }
        // 这里 tmp 指向了最后一个了
        tmp->next = s;
    }

    // 返回一个可用的位置
    // 因为是新空间，直接返回第一个位置
    set_bit(s, 0, 1);
    s->num_free--;
    return s->mem_pos;
}

// slab 内存释放
// mem_pos 即为带头地址
void slab_free(int mem_pos, void *start_of_memory)
{
    // 先求出对象的大小
    int size = *((uint64_t *)((char *)start_of_memory + mem_pos));
    printf("slab free pos: %d, size: %d\n", mem_pos, size);

    // 在链表中查找
    struct slab_info *p = slabs;

    while (p) {
        // 同一个 size 的可能存在多个块，因为可能同一个 size 的块多于 64 个
        // 所以还得考虑 mem_pos 是不是在当前查询的块的区间
        if (p->size == size) {
            // 是当前块的   同样大小的有很多，但是要通过判断地址来确认就是这一个
            if (mem_pos >= p->mem_pos && mem_pos < p->mem_pos + ((p->size + HEADER_SIZE) * N_OBJS_PER_SLAB)) {
                // 且在当前区间的
                break;
            }
        }

        p = p->next;
    }

    if (! p) {
        printf("坏了，没有找到释放的slab.\n");
        return;
    }

    // 找到了, 然后看保存在哪个位置
    int i;
    for (i = 0; i < N_OBJS_PER_SLAB; i++) {
        if ((p->mem_pos + i * (HEADER_SIZE + size)) == mem_pos) {
            // 找到了对应的位置, 标记为释放
            printf("slab free pos: %d, index: %d\n", p->mem_pos, i);
            set_bit(p, i, 0);
            // 可用数加1
            p->num_free++;
            break;
        }
    }

    if (i == N_OBJS_PER_SLAB) {
        // 在 p 中没有找到对应的位置的相关信息
        printf("坏了，没有找到释放的内存位置: %d.\n", mem_pos);
        return;
    }

    if (p->num_free == N_OBJS_PER_SLAB) {
        // 如果已经全部没使用了，则释放
        struct slab_info *tmp = slabs;

        // 如果 p 是首元素，则直接指向下一个
        if (slabs == p) {
            slabs = slabs->next;
        } else {
            // 找到 p 的前一元素
            while (tmp && tmp->next != p) {
                tmp = tmp->next;
            }

            // 从链表中删除 p
            tmp->next = p->next;
        }
// 因为一开始是包括headsize 现在减去
        buddy_free(p->mem_pos-HEADER_SIZE, start_of_memory);
        free(p);
    }
}
