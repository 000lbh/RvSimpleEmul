#pragma once

#include <cstdint>
#include <vector>
#include <set>

#include "RvMem.h"

constexpr const char *RVREGABINAME[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0",
    "t1", "t2", "s0", "s1", "a0", "a1",
    "a2", "a3", "a4", "a5", "a6", "a7",
    "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4",
    "t5", "t6"
};

class RvReg {
    uint64_t reg_u[32];
public:
    uint64_t pc;
    uint64_t &ra;
    uint64_t &sp;
    uint64_t &gp;
    uint64_t &tp;
    uint64_t &t0;
    uint64_t &t1;
    uint64_t &t2;
    uint64_t &s0;
    uint64_t &fp;
    uint64_t &s1;
    uint64_t &a0;
    uint64_t &a1;
    RvReg();
    RvReg(const RvReg &other);
    uint64_t get(uint8_t id) const;
    int64_t get_i(uint8_t id) const;
    void set(uint8_t id, uint64_t value);
    uint64_t operator[](uint8_t id) const;
    uint64_t &operator[](uint8_t id);
};

class RvBaseCpu {
protected:
    std::set<uint64_t> breakpoint;
public:
    RvMem &mem;
    RvReg reg;
    RvBaseCpu(RvMem &mem, const RvReg &reg);
    virtual bool add_breakpoint(uint64_t addr);
    virtual std::vector<uint64_t> get_breakpoint();
    bool find_breakpoint(uint64_t addr);
    virtual bool remove_breakpoint(uint64_t addr);

    /* step: run an instruction
     * if it fails, an exception will be thrown
     * returns nothing
     */
    virtual void step() = 0;

    /* exec: run the processor
     * if cycle == 0, run until processor halt,
     * encounter a breakpoint or an exception.
     * returns executed instructions count
     */
    virtual uint64_t exec(uint64_t cycle = 0, bool no_bp = false) = 0;
};

class RvSimpleCpu : public RvBaseCpu {
public:
    RvSimpleCpu(RvMem &mem, const RvReg &reg);
    void step() override;
    uint64_t exec(uint64_t cycle = 0, bool no_bp = false) override;
};
