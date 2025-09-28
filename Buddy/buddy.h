//
// Created by kyogre223 on 28/9/25.
//

#include <cstddef>
#include <assert.h>
#include <cstdint>
#include <unistd.h>
#include <cstring>
#include <map>

#ifndef BENCHMARKS_TESTS_BUDDY_H
#define BENCHMARKS_TESTS_BUDDY_H
namespace buddy {

    // available sizes: 8 bytes, 16 bytes, 32 bytes, .... , 256 bytes;
    // 2^3 ~ 2^8;
    constexpr std::size_t BUCKET_COUNT = 8 - 3 + 1;
    constexpr std::size_t MAX_ALLOC = 512;
    /*
     * bucket 0 = 512 bytes
     * bucket 1 = 256 bytes
     * .....
     *
     */
    struct list_head {
        struct list_head* next;
        struct list_head* prev;
    };
    struct header {
        std::size_t id;
    };

    constexpr static std::size_t get_max_idx() {
        std::size_t idx = 0;
        for (int i = 0; i < 5; ++i) {
            idx = 2 * idx + 2;
        }
        return idx;
    }

    list_head buckets[BUCKET_COUNT];
    constexpr size_t Idx = 2;
    constexpr size_t MaxIdx = get_max_idx();
    // what is the size of our map here ?
    bool flat_map[MaxIdx + 1];

    std::uintptr_t base_ptr;// original point of brk(0);
    std::uintptr_t start;
    std::uintptr_t max_ptr;
    std::map<std::uintptr_t, std::size_t>mp; // abit lazy way to do it

    static void init_base_ptr() {
        std::memset(flat_map, 0, sizeof(flat_map));
        void* cur = sbrk(0);
        start = reinterpret_cast<std::uintptr_t>(cur);
        std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(cur);
        std::uintptr_t aligned = (addr + 16) & ~(15);
        if (aligned > addr) {
            std::ptrdiff_t diff = aligned - addr;
            sbrk(diff);
        }

        base_ptr = aligned;
        assert((base_ptr & 7) == 0);
        max_ptr = reinterpret_cast<std::uintptr_t>(sbrk(512));
    }

    static std::size_t get_bucket_idx(std::size_t required) {
        int bucket = 5;
        std::size_t sz = 16;
        while (bucket >= 0 && sz < required) {
            --bucket;
            sz *= 2;
        }
        return bucket;
    }

    static void init_free_lists() {
        for (std::uint8_t i = 0; i < BUCKET_COUNT; ++i) {
            buckets[i].next = &buckets[i];
            buckets[i].prev = &buckets[i];
        }
    }

    static void list_push(list_head* list, list_head* entry) {
        entry->next = list;
        entry->prev = list->prev;
        list->prev->next = entry;
        list->prev = entry;
    }

    static void* list_pop(list_head* list) {
        if (list->prev == list) return nullptr;
        list_head* node = list->prev;
        node->prev->next = list;
        list->prev = node->prev;
        return node;
    }

    static inline bool list_empty(list_head* list) { return list->next == list; }

    std::size_t level_to_size(uint8_t level) {
        if (level == 0) return 512;
        if (level == 1) return 256;
        if (level == 2) return 128;
        if (level == 3) return 64;
        if (level == 4) return 32;
        if (level == 5) return 16;
        assert(false);
    }

    static bool chunk_impl(uint8_t level,std::size_t idx, std::uintptr_t ptr, std::size_t& required) {

        if (level == required) {
            if (flat_map[idx] == 0) {
                // put inside the bucket;
                list_push(&buckets[required], reinterpret_cast<list_head*>(ptr));
                flat_map[idx] = 1;
                mp[ptr] = idx;
                return true;
            }
            return false;
        }
        std::size_t left = (idx << 1) + 1;
        std::size_t size = level_to_size(level);
        std::uintptr_t addr1 = ptr;
        std::uintptr_t addr2 = ptr + size/2;
        std::size_t right = (idx << 1) + 2;
        if (chunk_impl(level + 1, left, addr1, required)) {
            flat_map[idx] = 1;
            return true;
        }
        if (chunk_impl(level + 1, right, addr2, required)) {
            flat_map[idx] = 1;
            return true;
        }

        return false;
    }
    static void chunk(std::size_t& bucket_idx) {
        std::size_t idx = 0;

        bool ret = chunk_impl(0, 0, base_ptr , bucket_idx);
        assert(ret);

    }
    void* malloc(std::size_t size) {
        if (size == 0) return nullptr;
        std::size_t requiredSize = size + 16;
        if (base_ptr == NULL) {
            // it is the first time we call malloc, init our lists;
            init_base_ptr();
            init_free_lists();

        }
        if (requiredSize >= MAX_ALLOC) {
            return nullptr;
        }
        std::size_t bucket_idx = get_bucket_idx(requiredSize);
        assert(bucket_idx < BUCKET_COUNT && bucket_idx >= 0);

        // if the bucket size is empty, then we need to colaesce, or break from the top first ?
        // to keep it simple, just break from the top;
        if (list_empty(&buckets[bucket_idx])) {
            // just try to break from the top;
            chunk(bucket_idx);
        }
        assert(!list_empty(&buckets[bucket_idx])); // now it should not be empty;

        void* ret = list_pop(&buckets[bucket_idx]);
        std::size_t* temp = reinterpret_cast<std::size_t*>(ret);
        *temp = mp[reinterpret_cast<std::uintptr_t>(ret)];

        return (void*)(reinterpret_cast<std::uintptr_t>(ret) + 16);
    }
}

#endif //BENCHMARKS_TESTS_BUDDY_H
