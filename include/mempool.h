/**
 * @file mempool.h
 * @brief 高性能固定大小内存池 (Fixed-Size Memory Pool)
 *
 * 本模块是 RPC 协程框架的内存基础设施，专为海量并发下高频的小对象（如 Coroutine, Context 等）分配场景设计。
 * 在 C10K+ 极限重压下，频繁调用原生 malloc/free 会引发“系统调用风暴”和“严重的内存碎片”，拖垮性能。
 * 本内存池通过“批量申请、按需分配、内嵌空闲链表”机制，提供了极致的 O(1) 原生内存分配/回收能力。
 *
 * 【核心架构机制】
 * 1. 零负载侵入式链表 (Embedded FreeList)
 *    利用 union 的内存共用特性，把 free_list 的 next 指针直接写在空闲的内存块中。
 *    这就意味着：管理 10 万个空闲内存块，不需要额外浪费哪怕 1 byte 的空间存储指针！
 *
 * 2. 内存分配与对象生命周期严格解耦
 *    坚持使用底层 C 语言的 malloc/free 提供 Raw Memory（纯粹字节流），绕过 new/delete 带来
 *    的不可控的构造/析构开销。将面向对象初始化的权力上抛给更高层的 ObjPool (基于 placement new)。
 *
 * 3. 连续内存预分配 (Chunk Allocation)
 *    拒绝零碎申请，每次枯竭时向 OS 申请一大块连续内存 (Chunk) 进行切分。
 *    这既极大降低了缺页中断 (Page Fault) 的频率，又利用了 CPU 缓存行 (Cache Line) 的空间局部性。
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
         * @note - 返回值指向的是可直接使用的对象内存
         * @note - 调用方必须保证释放时调用 FreeAMemBlock()
         */
        void *AllocAMemBlock();

        /**
         * @brief 释放一块内存
         * @param block 由 AllocAMemBlock() 返回的地址
         *
         * @note - 这里只是把块挂回空闲链表，并不真正归还给系统
         * @note - 系统内存会在 MemPool 析构时统一释放
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