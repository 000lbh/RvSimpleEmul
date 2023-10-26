#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <set>
#include <unordered_map>

class RvReg;

#include "RvMem.h"
#include "RvInst.h"
#include "RvBranchPred.hpp"

constexpr const char *RVREGABINAME[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0",
    "t1", "t2", "s0", "s1", "a0", "a1",
    "a2", "a3", "a4", "a5", "a6", "a7",
    "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4",
    "t5", "t6"
};

class RvReg {
    std::array<uint64_t ,32> reg_u;
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
    RvReg &operator=(const RvReg &other);
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

class RvMultiCycleCpu : public RvBaseCpu {
    uint64_t executed_cycles;
    uint64_t executed_insts;
    std::unordered_map<std::string, uint64_t> inst_stat;
public:
    RvMultiCycleCpu(RvMem &mem, const RvReg &reg);
    void step() override;
    uint64_t exec(uint64_t cycle = 0, bool no_bp = false) override;
    uint64_t get_cycle_count() const;
    double get_cpi() const;
    const std::unordered_map<std::string, uint64_t> &get_inst_stat() const;
    void reset_stat();
};

class RvPipelineCpu : public RvBaseCpu{
    friend class RvBranchPred;

    // Statistics information;
    uint64_t executed_cycles;
    uint64_t executed_insts;
    uint64_t branch_insts;
    uint64_t branch_miss;
    uint64_t squashed_insts;
    std::unordered_map<std::string, uint64_t> inst_stat;

    // PC for fetching
    uint64_t fetch_pc;

    // Branch Predictor
    RvBranchPred &predictor;

    // Stages instructions
    std::unique_ptr<RvInst> fetch_inst;
    std::unique_ptr<RvInst> decode_inst;
    std::unique_ptr<RvInst> exec_inst;
    std::unique_ptr<RvInst> exec_mul_inst;
    std::unique_ptr<RvInst> mem_inst;
    std::unique_ptr<RvInst> wb_inst;

    // Stages stall
    uint64_t fetch_cycle;
    uint64_t decode_cycle;
    uint64_t exec_cycle;
    uint64_t mem_cycle;

    // Memory access info
    std::optional<RvMemAcc> mem_acc_info;

    // Stages registers
    RvReg fetch_reg;
    RvReg decode_reg;
    RvReg exec_reg;
    RvReg mem_reg;
    RvReg wb_reg;

    // Stage invalidate
    bool fetch_invd;
    bool decode_invd;
    bool exec_invd;
    bool mem_invd;
    bool wb_invd;

    // Stages operation
    void stage_fetch();
    void stage_decode();
    void stage_exec();
    void stage_mem();
    void stage_wb();
public:
    struct status_t {
        std::string fetch_inst;
        std::string decode_inst;
        std::string exec_inst;
        std::string mem_inst;
        std::string wb_inst;
        uint64_t fetch_cycle;
        uint64_t decode_cycle;
        uint64_t exec_cycle;
        uint64_t mem_cycle;
        uint64_t wb_cycle;
    };
    RvPipelineCpu(RvMem &mem, const RvReg &reg, RvBranchPred &branch_pred);
    void step() override;
    uint64_t exec(uint64_t cycle = 0, bool no_bp = false);
    double get_missrate() const;
    double get_cpi() const;
    uint64_t get_cycle_count() const;
    uint64_t get_inst_count() const;
    uint64_t get_branch_count() const;
    uint64_t get_branch_miss() const;
    const decltype(inst_stat) &get_inst_stat() const;
    status_t get_internal_status() const;
    void reset_stat();
};
