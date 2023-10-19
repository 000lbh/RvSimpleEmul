#include "RvMem.h"

RvMem::RvMem()
{
    return;
}

uint32_t RvMem::fetch(uint64_t addr)
{
    if (addr & 1)
        throw RvMisAlign(addr);
    try {
        auto &entry = page_table.at(addr >> 12);
        if (!(entry.perm & P_EXEC))
            throw RvAccVio(addr);
        return *reinterpret_cast<uint32_t *>(reinterpret_cast<char *>(entry.addr) + (addr & 0xfff));
    }
    catch (std::out_of_range) {
        throw RvAccVio(addr);
    }
}

bool RvMem::new_page(uint64_t addr_hint, int perm) {
    if (page_table.find(addr_hint >> 12) != page_table.end())
        return false;
    void *buf = ::malloc(1 << 12);
    if (!buf)
        return false;
    owned_page.insert(buf);
    return map_page(addr_hint, perm, buf);
}

bool RvMem::map_page(uint64_t addr_hint, int perm, void *phy_addr) {
    if (page_table.find(addr_hint >> 12) != page_table.end())
        return false;
    page_table.insert({ addr_hint >> 12, {phy_addr, perm} });
    return true;
}

bool RvMem::delete_page(uint64_t addr) {
    if (page_table.find(addr >> 12) == page_table.end())
        return false;
    if (owned_page.find(page_table[addr >> 12].addr) == owned_page.end())
        return false;
    ::free(page_table[addr >> 12].addr);
    owned_page.erase(page_table[addr >> 12].addr);
    page_table.erase(addr >> 12);
    return true;
}

bool RvMem::unmap_page(uint64_t addr) {
    if (page_table.find(addr >> 12) == page_table.end())
        return false;
    page_table.erase(addr >> 12);
    return true;
}

RvMem::MemWrapper RvMem::operator[](uint64_t addr)
{
    if (page_table.find(addr >> 12) == page_table.end())
        throw RvAccVio(0);
    auto &entry{ page_table[addr >> 12] };
    return MemWrapper(reinterpret_cast<char *>(entry.addr) + (addr & 0xfff), entry.perm);
}

RvMem::~RvMem()
{
    for (void *i : owned_page)
        ::free(i);
}
