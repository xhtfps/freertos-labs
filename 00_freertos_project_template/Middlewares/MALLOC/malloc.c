/**
 * @file Middlewares/MALLOC/malloc.c
 * @brief 多内存池动态内存分配器模块。
 * @details
 * - 管理内部 SRAM、CCM SRAM 和外部 SRAM 多个内存池。
 * - MemoryAllocator 对象集中维护内存表、初始化状态和分配算法。
 * - 兼容旧分配 API，同时支持对象化调用。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "./MALLOC/malloc.h"

#define MEM2_BASE_ADDR      ((uint32_t)0x10000000U + MEM2_RESERVED_SIZE)
#define MEM2_MAP_ADDR       (MEM2_BASE_ADDR + MEM2_MAX_SIZE)
#define MEM3_BASE_ADDR      ((uint32_t)0x68000000U)
#define MEM3_MAP_ADDR       (MEM3_BASE_ADDR + MEM3_MAX_SIZE)

/* 内存池(32字节对齐) */
__align(32) uint8_t  mem1base[MEM1_MAX_SIZE];                                                   /* 内部SRAM内存池 */

/* 内存管理表 */
uint16_t  mem1mapbase[MEM1_ALLOC_TABLE_SIZE];                                                   /* 内部SRAM内存池MAP */

/* 内存管理参数 */
const uint32_t  memtblsize[SRAMBANK] = {MEM1_ALLOC_TABLE_SIZE, MEM2_ALLOC_TABLE_SIZE, MEM3_ALLOC_TABLE_SIZE};   /* 内存表大小 */
const uint32_t  memblksize[SRAMBANK] = {MEM1_BLOCK_SIZE, MEM2_BLOCK_SIZE, MEM3_BLOCK_SIZE};                     /* 内存块大小 */
const uint32_t  memsize[SRAMBANK]    = {MEM1_MAX_SIZE, MEM2_MAX_SIZE, MEM3_MAX_SIZE};                           /* 内存总大小 */

static void MemoryAllocator_Init(MemoryAllocator *self, uint8_t memx);
static uint16_t MemoryAllocator_Perused(MemoryAllocator *self, uint8_t memx);
static uint32_t MemoryAllocator_MallocOffset(MemoryAllocator *self, uint8_t memx, uint32_t size);
static uint8_t MemoryAllocator_FreeOffset(MemoryAllocator *self, uint8_t memx, uint32_t offset);
static void *MemoryAllocator_Malloc(MemoryAllocator *self, uint8_t memx, uint32_t size);
static void MemoryAllocator_Free(MemoryAllocator *self, uint8_t memx, void *ptr);
static void *MemoryAllocator_Realloc(MemoryAllocator *self, uint8_t memx, void *ptr, uint32_t size);
static void MemoryAllocator_Construct(MemoryAllocator *self);

static const MemoryAllocatorOps s_memory_allocator_ops =
{
    MemoryAllocator_Init,
    MemoryAllocator_Perused,
    MemoryAllocator_MallocOffset,
    MemoryAllocator_FreeOffset,
    MemoryAllocator_Malloc,
    MemoryAllocator_Free,
    MemoryAllocator_Realloc
};

MemoryAllocator g_memory_allocator;

/* 内存管理控制器 */
struct _m_mallco_dev mallco_dev=
{
    my_mem_init,                            /* 内存初始化 */
    my_mem_perused,                         /* 内存使用率 */
    mem1base, (uint8_t *)MEM2_BASE_ADDR, (uint8_t *)MEM3_BASE_ADDR,           /* 内存池 */
    mem1mapbase, (uint16_t *)MEM2_MAP_ADDR, (uint16_t *)MEM3_MAP_ADDR,        /* 内存管理状态表 */
    0,0,0,                                  /* 内存未就绪 */
};

static void MemoryAllocator_Construct(MemoryAllocator *self)
{
    if ((self == NULL) || (self->ops != NULL))
    {
        return;
    }

    self->membase[SRAMIN] = mem1base;
    self->membase[SRAMCCM] = (uint8_t *)MEM2_BASE_ADDR;
    self->membase[SRAMEX] = (uint8_t *)MEM3_BASE_ADDR;
    self->memmap[SRAMIN] = mem1mapbase;
    self->memmap[SRAMCCM] = (uint16_t *)MEM2_MAP_ADDR;
    self->memmap[SRAMEX] = (uint16_t *)MEM3_MAP_ADDR;
    self->memrdy[SRAMIN] = mallco_dev.memrdy[SRAMIN];
    self->memrdy[SRAMCCM] = mallco_dev.memrdy[SRAMCCM];
    self->memrdy[SRAMEX] = mallco_dev.memrdy[SRAMEX];
    self->memtblsize = memtblsize;
    self->memblksize = memblksize;
    self->memsize = memsize;
    self->ops = &s_memory_allocator_ops;
}

/**
 * @brief       内存拷贝
 * @param       *des : 目的地址
 * @param       *src : 源地址
 * @param       n    : 要拷贝的字节数
 * @retval      无
 */
void mymemcpy(void *des, void *src, uint32_t n)
{
    uint8_t  *xdes = des;
    uint8_t  *xsrc = src;
    while (n--)*xdes++ = *xsrc++;
}

/**
 * @brief       内存设置
 * @param       *s    : 内存起始地址
 * @param       c     : 要设置的值
 * @param       count : 要设置的字节数
 * @retval      无
 */
void mymemset(void *s,uint8_t c, uint32_t count)
{
    uint8_t  *xs = s;
    while( count-- )*xs++ = c;
}

/**
 * @brief       内存管理初始化
 * @param       memx : 内存编号
 * @retval      无
 */
static void MemoryAllocator_Init(MemoryAllocator *self, uint8_t memx)
{
    if ((self == NULL) || (memx >= SRAMBANK))
    {
        return;
    }

    mymemset(self->memmap[memx], 0, self->memtblsize[memx] * 2);  /* 内存状态表清零 */
    mymemset(self->membase[memx], 0, self->memsize[memx]);        /* 内存池清零 */
    self->memrdy[memx] = 1;                                       /* 内存初始化OK */
    mallco_dev.memrdy[memx] = 1;
}

/**
 * @brief       获取内存使用率
 * @param       memx : 内存编号
 * @retval      使用率(放大10倍,0~1000,对应0.0%~100.0%)
 */
static uint16_t MemoryAllocator_Perused(MemoryAllocator *self, uint8_t memx)
{
    uint32_t  used = 0;
    uint32_t  i;

    if ((self == NULL) || (memx >= SRAMBANK))
    {
        return 0;
    }

    for (i = 0; i < self->memtblsize[memx]; i++)
    {
        if (self->memmap[memx][i])
        {
            used++;
        }
    }
    return (used * 1000) / (self->memtblsize[memx]);
}

/**
 * @brief       内存分配(内部调用)
 * @param       memx : 内存编号
 * @param       size : 要分配的内存大小(字节)
 * @retval      内存偏移地址
 *   @arg       0 ~ 0XFFFFFFFE : 有效偏移地址
 *   @arg       0XFFFFFFFF     : 分配失败
 */
static uint32_t MemoryAllocator_MallocOffset(MemoryAllocator *self, uint8_t memx, uint32_t size)
{
    signed long offset = 0;
    uint32_t  nmemb;        /* 需要的内存块数 */
    uint32_t  cmemb = 0;    /* 连续空闲块数 */
    uint32_t  i;

    if ((self == NULL) || (memx >= SRAMBANK))
    {
        return 0xFFFFFFFF;
    }
    
    if (!self->memrdy[memx])
    {
        self->ops->init(self, memx);  /* 未初始化,先执行初始化 */
    }
    if(size == 0)
    {
        return 0xFFFFFFFF;      /* 无需分配 */
    }
    
    nmemb = size / self->memblksize[memx];                       /* 计算需要多少个内存块 */
    if (size % self->memblksize[memx])
    {
        nmemb++;
    }
    /* 分配算法：从管理表尾部向前寻找连续空闲块。
     * 找到后把这 nmemb 个表项都写成相同的块数，释放时只需读取首块即可知道整段长度。
     * 返回 offset 而非真实指针，是为了让内部算法和不同物理内存池的基地址解耦。
     */
    for (offset = self->memtblsize[memx] - 1; offset >= 0; offset--)  /* 从末尾查找 */
    {
        if (!self->memmap[memx][offset])
        {
            cmemb++;        /* 连续空闲块计数 */
        }
        else 
        {
            cmemb = 0;      /* 遇到已占用块,清零 */
        }
        if (cmemb == nmemb) /* 找到连续nmemb个空闲块 */
        {
            for (i = 0; i < nmemb; i++)                         /* 标记为已占用 */
            {
                self->memmap[memx][offset + i] = nmemb;
            }
            return (offset * self->memblksize[memx]);           /* 返回偏移地址 */
        }
    }
    return 0XFFFFFFFF;      /* 未找到足够连续内存 */
}

/**
 * @brief       内存释放(内部调用)
 * @param       memx   : 内存编号
 * @param       offset : 内存地址偏移
 * @retval      释放结果
 *   @arg       0, 释放成功;
 *   @arg       1, 未初始化;
 *   @arg       2, 偏移越界;
 */
static uint8_t MemoryAllocator_FreeOffset(MemoryAllocator *self, uint8_t memx, uint32_t offset)
{
    int i;

    if ((self == NULL) || (memx >= SRAMBANK))
    {
        return 2;
    }
    
    if (!self->memrdy[memx])        /* 未初始化,先初始化 */
    {
        self->ops->init(self, memx);
        return 1;                   /* 未初始化 */
    }
    if (offset < self->memsize[memx])     /* 偏移在内存范围内 */
    {
        /* memmap[index] 保存分配时写入的连续块数。
         * 因此外部必须传入当初 malloc 返回的起始指针/偏移，不支持释放中间地址。
         */
        int index = offset / self->memblksize[memx];      /* 计算对应块索引 */
        int nmemb = self->memmap[memx][index];            /* 获取占用块数 */
        for (i = 0; i < nmemb; i++)                 /* 释放对应块 */
        {
            self->memmap[memx][index + i] = 0;
        }
        return 0;
    }
    else
    {
        return 2;                   /* 偏移越界 */
    }
}

/**
 * @brief       内存释放(外部调用)
 * @param       memx : 内存编号
 * @param       ptr  : 内存指针
 * @retval      无
 */
static void MemoryAllocator_Free(MemoryAllocator *self, uint8_t memx, void *ptr)
{
    uint32_t  offset;
    
    if ((self == NULL) || (memx >= SRAMBANK) || (ptr == NULL))
    {
        return;                 /* 空指针直接返回 */
    }
    offset = (uint32_t)ptr - (uint32_t )self->membase[memx];
    self->ops->free_offset(self, memx, offset);  /* 释放内存 */
}

/**
 * @brief       内存分配(外部调用)
 * @param       memx : 内存编号
 * @param       size : 要分配的内存大小(字节)
 * @retval      分配到的内存指针.
 */
static void *MemoryAllocator_Malloc(MemoryAllocator *self, uint8_t memx, uint32_t size)
{
    uint32_t  offset;

    if ((self == NULL) || (memx >= SRAMBANK))
    {
        return NULL;
    }
    
    offset = self->ops->malloc_offset(self, memx, size);

    if (offset == 0XFFFFFFFF)
    {
        return NULL;
    }
    else
    {
        return (void*)((uint32_t)self->membase[memx] + offset);
    }
}

/**
 * @brief       重新分配内存(外部调用)
 * @param       memx : 内存编号
 * @param       *ptr : 旧内存指针
 * @param       size : 新内存大小(字节)
 * @retval      新内存指针.
 */
static void *MemoryAllocator_Realloc(MemoryAllocator *self, uint8_t memx, void *ptr, uint32_t size)
{
    uint32_t  offset;

    if ((self == NULL) || (memx >= SRAMBANK))
    {
        return NULL;
    }
    
    offset = self->ops->malloc_offset(self, memx, size);
    if(offset == 0XFFFFFFFF)
    {
        return NULL;
    }
    else
    {
        /* 兼容旧实现：按新 size 复制旧指针内容。
         * 调用者应确保 size 不超过旧分配块的有效数据长度；该分配器不额外保存旧请求字节数。
         */
        mymemcpy((void*)((uint32_t )self->membase[memx] + offset), ptr, size); /* 拷贝旧数据到新内存 */
        self->ops->free(self, memx, ptr);                                      /* 释放旧内存 */
        return (void*)((uint32_t )self->membase[memx] + offset);               /* 返回新内存指针 */
    }
}

void my_mem_init(uint8_t memx)
{
    MemoryAllocator_Construct(&g_memory_allocator);
    g_memory_allocator.ops->init(&g_memory_allocator, memx);
}

uint16_t my_mem_perused(uint8_t memx)
{
    MemoryAllocator_Construct(&g_memory_allocator);
    return g_memory_allocator.ops->perused(&g_memory_allocator, memx);
}

uint32_t my_mem_malloc(uint8_t memx, uint32_t size)
{
    MemoryAllocator_Construct(&g_memory_allocator);
    return g_memory_allocator.ops->malloc_offset(&g_memory_allocator, memx, size);
}

uint8_t my_mem_free(uint8_t memx, uint32_t offset)
{
    MemoryAllocator_Construct(&g_memory_allocator);
    return g_memory_allocator.ops->free_offset(&g_memory_allocator, memx, offset);
}

void myfree(uint8_t memx, void *ptr)
{
    MemoryAllocator_Construct(&g_memory_allocator);
    g_memory_allocator.ops->free(&g_memory_allocator, memx, ptr);
}

void *mymalloc(uint8_t memx, uint32_t size)
{
    MemoryAllocator_Construct(&g_memory_allocator);
    return g_memory_allocator.ops->malloc(&g_memory_allocator, memx, size);
}

void *myrealloc(uint8_t memx, void *ptr, uint32_t size)
{
    MemoryAllocator_Construct(&g_memory_allocator);
    return g_memory_allocator.ops->realloc(&g_memory_allocator, memx, ptr, size);
}
