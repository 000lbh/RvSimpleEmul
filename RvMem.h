#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <concepts>

#include "RvExcept.hpp"

class RvMem {
    struct pg_entry {
        void *addr;
        int perm;
    };
    std::map<uint64_t, pg_entry> page_table;
    std::set<void *> owned_page;
    RvMem(const RvMem &) = delete;
    RvMem(RvMem &&) = delete;
    RvMem &operator=(const RvMem &) = delete;
    RvMem &operator=(RvMem &&) = delete;
public:
    enum {
        P_READ = 1,
        P_WRITE = 2,
        P_EXEC = 4
    };
    class MemWrapper {
        friend class RvMem;
        void *data;
        int perm;
        MemWrapper(void *data, int perm)
            : data{ data }
            , perm{ perm }
        {
            return;
        }
        MemWrapper(const MemWrapper &) = delete;
        MemWrapper(MemWrapper &&) = delete;
        MemWrapper &operator=(const MemWrapper &) = delete;
        MemWrapper &operator=(MemWrapper &&) = delete;
    public:
        template<std::integral T>
        T &operator=(T data) 
        {
            if (!(perm & P_WRITE))
                throw RvAccVio(0);
            return *reinterpret_cast<T *>(this->data) = data;
        }
        template <std::integral T>
        operator T()
        {
            if (!(perm & P_READ))
                throw RvAccVio(0);
            return *reinterpret_cast<T *>(this->data);
        }
    };
    RvMem();
    uint32_t fetch(uint64_t addr);
    // RvMem owns the newly allocated page
    bool new_page(uint64_t addr_hint, int perm);
    // RvMem doesn't own the page
    bool map_page(uint64_t addr_hint, int perm, void *phy_addr);
    // remove mapping and delete
    bool delete_page(uint64_t addr);
    // just unmap
    bool unmap_page(uint64_t addr);
    MemWrapper operator[](uint64_t addr);
    ~RvMem();
};