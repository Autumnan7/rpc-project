/**

* @brief 为什么本内存池使用 malloc/free 而不是 new/delete？
*
* 本模块属于“底层内存管理组件”，核心职责是管理“原始内存（raw memory）”，
* 而不是对象的构造与析构。因此选择使用 C 风格的 malloc/free，而非 C++ 的 new/delete。
*
* 主要原因如下：
*
* 1. 控制粒度更低（核心原因）
* malloc/free 只负责“分配/释放字节内存”，不会调用构造或析构函数；
* 而 new/delete 会隐式调用构造/析构函数，不利于实现通用内存池。
*
* 2. 避免不必要的开销
* 内存池通常用于高频小对象分配（如协程、任务等），
* 如果使用 new/delete，会引入额外的构造/析构开销，影响性能。
*
* 3. 支持对象与内存解耦
* 内存池只提供“内存块”，对象的构造应由上层通过 placement new 完成：
* ```
     void* mem = pool.AllocAMemBlock();
  ```
* ```
     Object* obj = new (mem) Object();
  ```
*
* 这样可以实现：
* * 延迟构造
* * 自定义生命周期管理
*
* 4. 更符合内存池设计模式
* 主流高性能内存池（如 STL 二级空间配置器、tcmalloc、jemalloc）
* 均采用 malloc 作为底层分配手段，再进行二次封装。
*
* 总结：
* malloc/free 用于“内存管理”
* new/delete 用于“对象管理”
*
* 本模块属于前者，因此采用 malloc/free。
  */

#pragma once

#include <cstddef> // std::size_t
#include <cstdlib> // std::malloc, std::free
#include <new>     // std::bad_alloc (可选)
#include "parameter.h"
#include "utils.h"
#include "logger.h"

namespace minico
{
    /**
     * @brief 内存块节点
     * @param next 空闲链表指针，指向下一个空闲块
     * @param data 该块可用数据区的起始地址（通过 union 与 next 共享内存）
     */
    struct MemBlockNode
    {
        // union 让所有成员共享同一内存区域，节省空间,成员之间互斥使用
        union
        {
            MemBlockNode *next;
            char data;
        };
    };

    /**
     * @brief 固定大小内存池
     *
     * 每个 MemPool 实例只管理一种固定大小的内存块，块大小由模板参数 objSize 决定。
     *
     * 分配策略：
     * 1. 优先从空闲链表中取块
     * 2. 如果空闲链表为空，则一次性向系统申请一批块
     * 3. 申请到的大块内存会被切分成多个固定块，并挂到空闲链表
     *
     * 适用场景：
     * - 频繁创建/销毁固定大小对象
     * - 如 Coroutine、Task、Node 等
     */
    template <std::size_t objSize>
    class MemPool
    {
    public:
        MemPool()
            : _freeListHead(nullptr),
              _mallocListHead(nullptr),
              _mallocTimes(0),
              objSize_(objSize < sizeof(MemBlockNode) ? sizeof(MemBlockNode) : objSize) // 确保块大小至少能容纳一个链表节点
        {
        }

        ~MemPool();

        DISALLOW_COPY_MOVE_AND_ASSIGN(MemPool);

        /**
         * @brief 分配一块内存
         * @return 返回块内数据区首地址；失败返回 nullptr
         *
         * 注意：
         * - 返回值指向的是可直接使用的对象内存
         * - 调用方必须保证释放时调用 FreeAMemBlock()
         */
        void *AllocAMemBlock();

        /**
         * @brief 释放一块内存
         * @param block 由 AllocAMemBlock() 返回的地址
         *
         * 注意：
         * - 这里只是把块挂回空闲链表，并不真正归还给系统
         * - 系统内存会在 MemPool 析构时统一释放
         */
        void FreeAMemBlock(void *block);

    private:
        /** 空闲链表头 */
        MemBlockNode *_freeListHead;

        /** 已向系统申请的大块内存链表头 */
        MemBlockNode *_mallocListHead;

        /** 已向系统申请内存的次数 */
        std::size_t _mallocTimes;

        /** 实际分配块大小（保证至少能容纳一个 MemBlockNode） */
        std::size_t objSize_;
    };

    template <std::size_t objSize>
    MemPool<objSize>::~MemPool()
    {
        // 释放所有曾向系统申请过的大块内存
        while (_mallocListHead != nullptr)
        {
            MemBlockNode *mallocNode = _mallocListHead;
            _mallocListHead = mallocNode->next;
            std::free(static_cast<void *>(mallocNode));
        }
    }

    template <std::size_t objSize>
    void *MemPool<objSize>::AllocAMemBlock()
    {
        // 如果空闲链表为空，则一次性申请一批块
        if (_freeListHead == nullptr)
        {
            // 每次申请的块数：基础数量 + 已申请次数（逐步增长）
            std::size_t mallocCnt = parameter::memPoolMallocObjCnt + _mallocTimes;

            // 额外加一个 MemBlockNode 作为“大块头结点”，总的内存大小 = 块数 * 块大小 + 大块头结点大小
            void *newMallocBlk = std::malloc(mallocCnt * objSize_ + sizeof(MemBlockNode));
            if (newMallocBlk == nullptr)
            {
                LOG_ERROR("MemPool malloc failed");
                return nullptr;
            }

            // 把这次申请的大块内存挂到“大块链表”的头部
            // malloc会返回 一个void指针，这里显示转换为MemBlockNode指针，方便后续链表操作
            MemBlockNode *mallocNode = static_cast<MemBlockNode *>(newMallocBlk); //
            mallocNode->next = _mallocListHead;
            _mallocListHead = mallocNode;

            // 跳过头结点：将指针按字节后移 sizeof(MemBlockNode)，使其指向可切分对象块的起始位置
            char *cur = static_cast<char *>(newMallocBlk) + sizeof(MemBlockNode);

            for (std::size_t i = 0; i < mallocCnt; ++i)
            {
                MemBlockNode *newNode = static_cast<MemBlockNode *>(static_cast<void *>(cur));
                newNode->next = _freeListHead;
                _freeListHead = newNode;
                cur += objSize_; // 因为objSize人为设定，所以要用cur来控制地址递增，确保每个块的起始地址正确对齐并且连续
            }

            ++_mallocTimes;
        }

        // 从空闲链表取一个块，retnode:return node
        MemBlockNode *retNode = _freeListHead;
        _freeListHead = _freeListHead->next;

        return &(retNode->data);
    }

    template <std::size_t objSize>
    void MemPool<objSize>::FreeAMemBlock(void *block)
    {
        if (block == nullptr)
        {
            return;
        }

        // block 必须是 AllocAMemBlock() 返回的地址
        // 将其重新挂回空闲链表
        MemBlockNode *newNode = static_cast<MemBlockNode *>(block);
        newNode->next = _freeListHead;
        _freeListHead = newNode;
    }

} // namespace minico