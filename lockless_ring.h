#pragma once
#include <climits>
#include <atomic>
#include <cstdint>
#include <iostream>

template <typename T>
struct LLR {
    std::atomic_uint16_t llr_prod_head_;
    std::atomic_uint16_t llr_prod_tail_;
    int                  llr_size_;
    int                  llr_mask_;

    uint64_t llr_drops_;
    uint64_t llr_prod_bufs_;
    uint64_t llr_prod_bytes_;
    uint64_t _pad0[11];

    std::atomic_uint16_t llr_cons_head_;
    std::atomic_uint16_t llr_cons_tail_;
    uint32_t             prod_offset_;
    uint32_t             cons_offset_;

    uint64_t _pad1[15];
#ifdef DEBUG_BUFRING
    struct mtx* br_lock;
#endif
    T* llr_ring[0];
};

template <typename T>
static int ring_enqueue(struct LLR<T>* llr, T* buf) {
    bool     success = false;
    uint16_t local_prod_head, local_cons_tail, local_prod_next;
    // uint32_t local_prod_offset;
    do {
        local_prod_head = llr->llr_prod_head_;  // 0 ~ UINT_MAX
        local_cons_tail = llr->llr_cons_tail_;  // 0 ~ UINT_MAX
        local_prod_next = local_prod_head + 1;
        // if (local_prod_next < local_prod_head) {  // flipping
        //     // need to update prod_offset.
        //     local_prod_offset =
        //         (llr->prod_offset_ + (uint32_t)(local_prod_head) + 1) %
        //         llr->llr_size_;  // for example: 65535 + 1 % 200 = 136, llr_ring[136] is the new start of ring.
        //     std::cout << "local prod head: " << local_prod_head << " local prod next: " << local_prod_next
        //               << " prod offset: " << llr->prod_offset_ << " local prod offset: " << local_prod_offset
        //               << " local cons tail: " << local_cons_tail << std::endl;
        // }
        if ((local_prod_next) % llr->llr_size_ == local_cons_tail % llr->llr_size_) {
            return -1;  // no enough bufs
        }
        success = llr->llr_prod_head_.compare_exchange_strong(
            local_prod_head, local_prod_next);  // could not use table_prod_next(0 ~ size - 1)
    } while (!success);

    llr->llr_ring[((uint32_t)local_prod_head) % llr->llr_size_] = buf;

    // if there are other enqueues in progress that preceeded us, we need to wait for them to complete.
    while (llr->llr_prod_tail_ != local_prod_head)
        ;

    llr->llr_prod_bufs_++;
    llr->llr_prod_bytes_ += sizeof(T*);
    llr->llr_prod_tail_ = local_prod_next;
    return 0;
}

template <typename T>
static T* ring_dequeue(struct LLR<T>* llr) {
    uint16_t local_cons_head, local_cons_next, local_prod_tail;
    T*       buf;
    bool     success = false;

    do {
        std::atomic_thread_fence(std::memory_order_acquire);
        // local_cons_head = std::atomic_load(&llr->llr_cons_head_, std::memory_order_acquire);
        local_cons_head = llr->llr_cons_head_;
        local_prod_tail = llr->llr_prod_tail_;
        local_cons_next = local_cons_head + 1;
        // local_cons_next may be zero.
        if(local_cons_head > local_cons_next) { // flipping
            // 
        }
        if (local_prod_tail % llr->llr_size_ == local_cons_head % llr->llr_size_) {
            // std::cout << "no enough bufs" << std::endl;
            return nullptr;  // no enough bufs
        }
        success = llr->llr_cons_head_.compare_exchange_strong(local_cons_head, local_cons_next);
    } while (!success);

    buf = llr->llr_ring[local_cons_head % llr->llr_size_];
    // llr->br_ring[cons_head] = nullptr;

    // if there are other dequeues in progress that preceeded us, we need to wait for them to complete.
    while (llr->llr_cons_tail_ != local_cons_head)
        ;
    llr->llr_prod_bufs_ -= sizeof(T*);
    llr->llr_cons_tail_ = local_cons_next;
    return buf;
}

template <typename T>
static struct LLR<T>* InitStructLLR(int size) {
    struct LLR<T>* llr = (struct LLR<T>*)calloc(sizeof(struct LLR<T>) + sizeof(T*) * size, 1);
    llr->llr_size_     = size;
    llr->llr_mask_     = size - 1;
    return llr;
}

template <typename T>
static __inline int ring_empty(struct LLR<T> * llr) {
    return (llr->llr_cons_head_ == llr->llr_prod_tail_);
}

template <typename T>
static __inline uint16_t ring_count(struct LLR<T>* llr) {
    return llr->llr_prod_tail_ - llr->llr_cons_head_;
}

// template <typename T>
// static __inline int ring_full(struct LLR<T>* llr) {
//     return (llr->llr_prod_head_ + 1) & llr->llr_mask_ == llr->llr_cons_tail_;
// }
