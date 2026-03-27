#pragma once
#include <type_traits>
#include "mempool.h"
/**
 * @brief 为用户所使用，在库中主要用在Coroutine实例的创建上
 * 对象池创建对象时，首先会从内存池中取出相应大小的块，内存池是与对象大小相关的，
 * 其中有一个空闲链表，每次分配空间都从空闲链表上取
 * 若空闲链表没有内容时，首先会分配（40 + 分配次数）* 对象大小的空间，然后分成一个个块挂在空闲链表上
 * 这里空闲链表节点没有使用额外的空间：效仿的stl的二级配置器中的方法，将数据和next指针放在了一个union中
 * 从内存池取出所需内存块后，会判断对象是否拥有non-trivial构造函数，没有的话直接返回，有的话使用placement new构造对象
 */
namespace minico
{
    // 对象池的实现
    template <class T>
    class ObjPool
    {
    public:
        ObjPool() {};
        ~ObjPool() {};

        DISALLOW_COPY_MOVE_AND_ASSIGN(ObjPool);

        /**
         * @brief 创建并返回一个对象池中的对象。
         * @note T 是否为平凡可构造类型进行编译期分发
         * @note - 如果 T 是平凡可构造的，则直接从内存池申请一块内存即可；
         * @note - 如果 T 需要构造函数，则在申请到的内存上执行 placement new 进行原地构造。
         */
        template <typename... Args>
        inline T *new_obj(Args... args);

        /**
         * @brief 释放对象池中的对象指针。
         *
         * @note 根据 T 是否为平凡可析构类型进行编译期分发：决定是否需要显式调用析构函数
         * @param obj 要释放的对象指针。
         */
        inline void delete_obj(void *obj);

    private:
        /**
         * @brief 平凡可构造类型的对象创建：直接从内存池申请一块内存即可。
         */
        template <typename... Args>
        inline T *new_aux(std::true_type, Args... args); // aux:auxiliary 辅助实现函数

        /**
         * @brief 非平凡可构造类型的对象创建
         * @note 先从内存池申请一块原始内存，然后在该内存上使用 placement new 构造对象。
         */
        template <typename... Args>
        inline T *new_aux(std::false_type, Args... args);

        /**
         * @brief 平凡可析构类型的对象释放
         * @note 不需要显式调用析构函数，直接归还内存块即可。
         */
        inline void delete_aux(std::true_type, void *obj);

        /**
         * @brief 非平凡可析构类型的对象释放：
         * @note 先显式调用析构函数，再将内存块归还给内存池。
         */
        inline void delete_aux(std::false_type, void *obj);

        MemPool<sizeof(T)> _memPool;
    };

    template <class T>
    template <typename... Args>
    inline T *ObjPool<T>::new_obj(Args... args)
    {
        return new_aux(
            std::integral_constant<bool, std::is_trivially_constructible<T>::value>(),
            args...);
    }

    template <class T>
    template <typename... Args>
    inline T *ObjPool<T>::new_aux(std::true_type, Args... args)
    {

        return static_cast<T *>(_memPool.AllocAMemBlock());
    }

    template <class T>
    template <typename... Args>
    inline T *ObjPool<T>::new_aux(std::false_type, Args... args)
    {
        // 先从内存池申请一块原始内存
        void *newPos = _memPool.AllocAMemBlock();

        // 在申请到的内存上使用 placement new 构造对象：
        // 1. 不额外分配内存；
        // 2. 直接在 newPos 指向的地址上调用 T 的构造函数；
        // 3. 返回构造完成后的对象指针。
        return new (newPos) T(args...);
    }

    template <class T>
    inline void ObjPool<T>::delete_obj(void *obj)
    {
        // 空指针直接返回，避免后续访问非法内存
        if (!obj)
        {
            return;
        }

        delete_aux(std::integral_constant<bool, std::is_trivially_destructible<T>::value>(), obj);
    }

    template <class T>
    inline void ObjPool<T>::delete_aux(std::true_type, void *obj)
    {
        _memPool.FreeAMemBlock(obj);
    }

    template <class T>
    inline void ObjPool<T>::delete_aux(std::false_type, void *obj)
    {
        (static_cast<T *>(obj))->~T();
        _memPool.FreeAMemBlock(obj);
    }
}
