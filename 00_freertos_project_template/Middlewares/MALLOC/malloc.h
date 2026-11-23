/**
 * @file Middlewares/MALLOC/malloc.h
 * @brief 多内存池动态内存分配器模块。
 * @details
 * - 管理内部 SRAM、CCM SRAM 和外部 SRAM 多个内存池。
 * - MemoryAllocator 对象集中维护内存表、初始化状态和分配算法。
 * - 兼容旧分配 API，同时支持对象化调用。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef __MALLOC_H
#define __MALLOC_H
#include "./SYSTEM/sys/sys.h"


#ifndef NULL
#define NULL 0
#endif

/* 定义三个内存池 */
#define     SRAMIN                  0                               /* 内部内存池 */
#define     SRAMCCM                 1                               /* CCM内存池(此部分SRAM仅仅CPU可以访问!!!) */
#define     SRAMEX                  2                               /* 外部内存池 */
#define     SRAMBANK                3                               /* 定义支持的SRAM块数 */

/* mem1内存参数设定.mem1完全处于内部SRAM里面 */
#define     MEM1_BLOCK_SIZE         32                              /* 内存块大小为32字节 */
#define     MEM1_MAX_SIZE           32*1024                        /* 最大管理内存 32K */
#define     MEM1_ALLOC_TABLE_SIZE   MEM1_MAX_SIZE/MEM1_BLOCK_SIZE   /* 内存表大小 */

/* mem2内存参数设定.mem2处于CCM,用于管理CCM(特别注意,这部分SRAM,仅CPU可以访问!!) */
#define     MEM2_BLOCK_SIZE         32                              /* 内存块大小为32字节 */
#define     MEM2_RESERVED_SIZE      (4 * 1024)                      /* Reserve CCM low memory for Keil RW_IRAM2 data/bss */
#define     MEM2_MAX_SIZE           (56 *1024)                      /* 最大管理内存56K */
#define     MEM2_ALLOC_TABLE_SIZE   MEM2_MAX_SIZE/MEM2_BLOCK_SIZE   /* 内存表大小 */

/* mem3内存参数设定.mem3是F407外扩SRAM */
#define MEM3_BLOCK_SIZE             32                              /* 内存块大小为32字节 */
#define MEM3_MAX_SIZE               963 *1024                       /* 最大管理内存963K, F407外扩SRAM大小1024KB */
#define MEM3_ALLOC_TABLE_SIZE       MEM3_MAX_SIZE/MEM3_BLOCK_SIZE   /* 内存表大小 */


/* 内存管理控制器 */
struct _m_mallco_dev
{
    void (*init)(uint8_t );             /* 初始化 */
    uint16_t (*perused)(uint8_t );       /* 内存使用率 */
    uint8_t *membase[SRAMBANK];         /* 内存池 管理SRAMBANK个区域的内存 */
    uint16_t *memmap[SRAMBANK];         /* 内存管理状态表 */
    uint8_t memrdy[SRAMBANK];           /* 内存管理是否就绪 */
};

extern struct _m_mallco_dev mallco_dev; /* 在mallco.c里面定义 */

typedef struct MemoryAllocator MemoryAllocator;

/**
 * @brief 多内存池分配器操作表。
 * @details 内部接口以 offset 为核心，外部接口以指针为核心，便于兼容旧分配器算法。
 */
typedef struct
{
    void (*init)(MemoryAllocator *self, uint8_t memx);                                  /**< 初始化指定内存池及其管理表。 */
    uint16_t (*perused)(MemoryAllocator *self, uint8_t memx);                           /**< 返回指定内存池使用率，单位 0.1%。 */
    uint32_t (*malloc_offset)(MemoryAllocator *self, uint8_t memx, uint32_t size);       /**< 分配内存并返回相对池起始地址的偏移。 */
    uint8_t (*free_offset)(MemoryAllocator *self, uint8_t memx, uint32_t offset);        /**< 按偏移释放一段已分配内存。 */
    void *(*malloc)(MemoryAllocator *self, uint8_t memx, uint32_t size);                 /**< 分配内存并返回真实指针。 */
    void (*free)(MemoryAllocator *self, uint8_t memx, void *ptr);                        /**< 按真实指针释放内存。 */
    void *(*realloc)(MemoryAllocator *self, uint8_t memx, void *ptr, uint32_t size);     /**< 重新分配并搬移旧数据。 */
} MemoryAllocatorOps;

/**
 * @brief 多内存池分配器对象。
 * @details
 * membase/memmap/memrdy 三组数组按 SRAMIN、SRAMCCM、SRAMEX 索引访问，
 * 因此新增内存池时必须同步更新 SRAMBANK 和三组容量表。
 */
struct MemoryAllocator
{
    uint8_t *membase[SRAMBANK];        /**< 各内存池起始地址。 */
    uint16_t *memmap[SRAMBANK];        /**< 各内存池块占用状态表。 */
    uint8_t memrdy[SRAMBANK];          /**< 各内存池初始化完成标志。 */
    const uint32_t *memtblsize;        /**< 各内存池管理表项数量。 */
    const uint32_t *memblksize;        /**< 各内存池单块大小。 */
    const uint32_t *memsize;           /**< 各内存池总容量。 */
    const MemoryAllocatorOps *ops;     /**< 分配器方法表。 */
};

extern MemoryAllocator g_memory_allocator;

void mymemset(void * s, uint8_t c, uint32_t count);         /* 设置内存 */
void mymemcpy(void * des, void * src, uint32_t n);          /* 复制内存 */
void my_mem_init(uint8_t memx);                             /* 内存管理初始化函数(外/内部调用) */
uint32_t my_mem_malloc(uint8_t memx, uint32_t size);        /* 内存分配(内部调用) */
uint8_t my_mem_free(uint8_t memx, uint32_t offset);         /* 内存释放(内部调用) */
uint16_t my_mem_perused(uint8_t memx);                      /* 获得内存使用率(外/内部调用) */

/* 用户调用函数 */
void myfree(uint8_t memx, void * ptr);                      /* 内存释放(外部调用) */
void *mymalloc(uint8_t memx, uint32_t size);                /* 内存分配(外部调用) */
void *myrealloc(uint8_t memx, void * ptr, uint32_t size);   /* 重新分配内存(外部调用) */
#endif
