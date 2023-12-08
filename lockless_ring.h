#pragma once
#include <climits>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <cmath>

template <typename T>
struct LLR {
    std::atomic_uint32_t llr_prod_head_;
    std::atomic_uint32_t llr_prod_tail_;
    int                  llr_size_;
    int                  llr_mask_;
    uint64_t             llr_drops_;
    uint64_t             llr_prod_bufs_;
    uint64_t             llr_prod_bytes_;
    std::atomic_uint32_t llr_cons_head_;
    std::atomic_uint32_t llr_cons_tail_;

    T* llr_ring[0];
};

template <typename T>
static int ring_enqueue(struct LLR<T>* llr, T* buf, int bulk_size = 1) {
    bool     success = false;
    uint32_t local_prod_head, local_cons_tail, local_prod_next;
    do {
        local_prod_head = llr->llr_prod_head_;
        local_cons_tail = llr->llr_cons_tail_;
        local_prod_next = local_prod_head + bulk_size;
        // check if there are enough bufs. free entries = mask + cons tail - prod head
        if (llr->llr_mask_ + local_cons_tail - local_prod_head < bulk_size) {
            return -1;
        }
        success = llr->llr_prod_head_.compare_exchange_strong(
            local_prod_head, local_prod_next);  // could not use table_prod_next(0 ~ size - 1)
    } while (!success);

    for (int i = 0; i < bulk_size; ++i) {
        llr->llr_ring[(local_prod_head + i) % llr->llr_size_] = buf + i;
    }

    // if there are other enqueues in progress that preceeded us, we need to wait for them to complete.
    while (llr->llr_prod_tail_ != local_prod_head)
        ;

    llr->llr_prod_bufs_++;
    llr->llr_prod_bytes_ += sizeof(T*);
    llr->llr_prod_tail_ = local_prod_next;
    return 0;
}

template <typename T>
static T* ring_dequeue(struct LLR<T>* llr, int bulk_size = 1) {
    uint32_t local_cons_head, local_cons_next, local_prod_tail;
    T*       buf;
    bool     success = false;

    do {
        local_cons_head = llr->llr_cons_head_;
        local_prod_tail = llr->llr_prod_tail_;
        local_cons_next = local_cons_head + bulk_size;
        // check if there are enough data, used entries = prod tail - cons head
        if (local_prod_tail - local_cons_head < bulk_size) {
            std::cout << "no enough data" << std::endl;
            return nullptr;  // no enough bufs
        }
        success = llr->llr_cons_head_.compare_exchange_strong(local_cons_head, local_cons_next);
    } while (!success);

    buf = llr->llr_ring[local_cons_head % llr->llr_size_];

    // if there are other dequeues in progress that preceeded us, we need to wait for them to complete.
    while (llr->llr_cons_tail_ != local_cons_head)
        ;
    llr->llr_prod_bufs_ -= sizeof(T*);
    llr->llr_cons_tail_ = local_cons_next;
    return buf;
}

template <typename T>
static struct LLR<T>* InitStructLLR(uint8_t power) {
    size_t         size = pow(2, power);
    struct LLR<T>* llr  = (struct LLR<T>*)calloc(sizeof(struct LLR<T>) + sizeof(T*) * size, 1);
    llr->llr_size_      = size;
    llr->llr_mask_      = size - 1;
    return llr;
}

template <typename T>
static __inline uint16_t ring_count(struct LLR<T>* llr) {
    return llr->llr_prod_tail_ - llr->llr_cons_head_;
}

template <typename T>
static __inline uint16_t ring_free(struct LLR<T>* llr) {
    return llr->llr_mask_ + llr->llr_cons_tail_ - llr->llr_prod_head_;
}

