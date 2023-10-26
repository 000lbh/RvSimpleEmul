#include "RvCpu.h"

#include <memory>
#include <iostream>
#include <span>
#include <typeinfo>
#include <string>

#include "RvInst.h"

using namespace std::string_literals;

#pragma region RvReg

RvReg::RvReg()
    : reg_u{}
    , pc{}
    , ra{ reg_u[1] }, sp{ reg_u[2] }, gp{ reg_u[3] }
    , tp{ reg_u[4] }, t0{ reg_u[5] }, t1{ reg_u[6] }
    , t2{ reg_u[7] }, s0{ reg_u[8] }, fp{ reg_u[8] }
    , s1{ reg_u[9] }, a0{ reg_u[10] }, a1{ reg_u[11] }
{
    return;
}

RvReg::RvReg(const RvReg &other)
    : reg_u{ other.reg_u }
    , pc{ other.pc }
    , ra{ reg_u[1] }, sp{ reg_u[2] }, gp{ reg_u[3] }
    , tp{ reg_u[4] }, t0{ reg_u[5] }, t1{ reg_u[6] }
    , t2{ reg_u[7] }, s0{ reg_u[8] }, fp{ reg_u[8] }
    , s1{ reg_u[9] }, a0{ reg_u[10] }, a1{ reg_u[11] }
{
    return;
}

uint64_t RvReg::get(uint8_t id) const {
    return reg_u[id];
}

int64_t RvReg::get_i(uint8_t id) const {
    // In cpp20, this behavior is defined
    return static_cast<int64_t>(reg_u[id]);
}

void RvReg::set(uint8_t id, uint64_t value) {
    if (id == 0)
        return;
    reg_u[id] = value;
}

uint64_t RvReg::operator[](uint8_t id) const {
    if (id == 0)
        return 0;
    else
        return reg_u[id];
}

uint64_t &RvReg::operator[](uint8_t id) {
    static uint64_t zero{};
    zero = 0;
    if (id == 0)
        return zero;
    return reg_u[id];
}

RvReg &RvReg::operator=(const RvReg &other) {
    pc = other.pc;
    reg_u = other.reg_u;
    reg_u[0] = 0;
    return *this;
}

#pragma endregion

#pragma region RvBaseCpu

RvBaseCpu::RvBaseCpu(RvMem &mem, const RvReg &reg)
    : mem(mem), reg(reg)
{
    return;
}

bool RvBaseCpu::add_breakpoint(uint64_t addr)
{
    if (breakpoint.find(addr) != breakpoint.end())
        return false;
    else
        breakpoint.insert(addr);
    return true;
}

std::vector<uint64_t> RvBaseCpu::get_breakpoint()
{
    return std::vector<uint64_t>(breakpoint.begin(), breakpoint.end());
}

bool RvBaseCpu::find_breakpoint(uint64_t addr)
{
    return breakpoint.find(addr) != breakpoint.end();
}

bool RvBaseCpu::remove_breakpoint(uint64_t addr)
{
    if (breakpoint.find(addr) == breakpoint.end())
        return false;
    else
        breakpoint.erase(addr);
    return true;
}

#pragma endregion

#pragma region RvSimpleCpu

RvSimpleCpu::RvSimpleCpu(RvMem &mem, const RvReg &reg)
    : RvBaseCpu(mem, reg)
{
    return;
}

void RvSimpleCpu::step()
{
    std::unique_ptr<RvInst> inst{ RvInst::decode(mem.fetch(reg.pc)) };
    std::cout << inst->name() << std::endl;
    try {
        inst->exec(reg);
        reg.pc += 4;
    }
    catch (const RvMemAcc &meminfo) {
        inst->mem(reg, mem, meminfo);
        reg.pc += 4;
    }
    catch (const RvCtrlFlowJmp &info) {
        reg.pc = info.target_addr;
    }
    catch (const RvHalt &e) {
        reg.pc += 4;
        throw;
    }
    catch (const RvSysCall &syscall) {
        reg.pc += 4;
        std::cerr << "Program issued a syscall." << std::endl;
        for (int i{ 0 }; i < 32; i++) {
            std::cout << RVREGABINAME[i] << "=0x" << std::hex << static_cast<uint64_t>(reg[i]) << std::endl;
        }
        std::cout << "pc=0x" << std::hex << reg.pc << std::endl;
        // Syscall no. at a7, return value at a0
    }
}

uint64_t RvSimpleCpu::exec(uint64_t cycle, bool no_bp)
{
    uint64_t inst_exec{};
    try {
        if (cycle == 0)
            for (;;) {
                if (!no_bp && breakpoint.find(reg.pc) != breakpoint.end())
                    break;
                step();
                inst_exec++;
            }
        else
            while (cycle--) {
                if (!no_bp && breakpoint.find(reg.pc) != breakpoint.end())
                    break;
                step();
                inst_exec++;
            }
    }
    catch (const RvHalt &e) {
        ;
    }
    catch (const RvException &e) {
        std::cerr << "We encountered an exception " << typeid(e).name() << ", " << e.what() << std::endl;
    }
    return inst_exec;
}

#pragma endregion

#pragma region RvMultiCycleCpu

RvMultiCycleCpu::RvMultiCycleCpu(RvMem &mem, const RvReg &reg)
    : RvBaseCpu(mem, reg)
    , executed_cycles{}
    , executed_insts{}
    , inst_stat{}
{
    return;
}

void RvMultiCycleCpu::step()
{
    std::unique_ptr<RvInst> inst1{ RvInst::decode(mem.fetch(reg.pc)) };
    std::unique_ptr<RvInst> inst2;
    try {
        executed_cycles += 2;
        try {
            inst2.reset(RvInst::decode(reg.pc + 4));
        }
        catch (const RvException &) {
            ;
        }
        if (inst2 && inst1->div_rem_ok(inst2.get())) {
            inst2->exec(reg);
            reg.pc += 4;
            executed_cycles += 4;
            executed_insts++;
            inst_stat[inst1->inst_name()]++;
        }
        inst1->exec(reg);
        // if have memory access or branch, subsequent code won't be executed
        executed_cycles += inst1->exec_cycle() + (dynamic_cast<RvSBInst *>(inst1.get()) ? 0 : 2);

        // Finally
        reg.pc += 4;
        executed_insts++;
        inst_stat[inst1->inst_name()]++;
    }
    catch (const RvMemAcc &meminfo) {
        inst1->mem(reg, mem, meminfo);
        executed_cycles += mem.mem_cycle() + inst1->exec_cycle();
        if (meminfo.rw == meminfo.READ)
            executed_cycles++;

        // Finally
        reg.pc += 4;
        executed_insts++;
        inst_stat[inst1->inst_name()]++;
    }
    catch (const RvCtrlFlowJmp &info) {
        reg.pc = info.target_addr;
        executed_cycles += inst1->exec_cycle();

        // Finally
        executed_insts++;
        inst_stat[inst1->inst_name()]++;
    }
    catch (const RvSysCall &syscall) {
        // Finally
        reg.pc += 4;
        executed_insts++;
        inst_stat[inst1->inst_name()]++;
    }
}

uint64_t RvMultiCycleCpu::exec(uint64_t cycle, bool no_bp)
{
    uint64_t inst_exec{};
    try {
        if (cycle == 0)
            for (;;) {
                if (!no_bp && breakpoint.find(reg.pc) != breakpoint.end())
                    break;
                step();
                inst_exec++;
            }
        else
            while (cycle--) {
                if (!no_bp && breakpoint.find(reg.pc) != breakpoint.end())
                    break;
                step();
                inst_exec++;
            }
    }
    catch (const RvHalt &e) {
        ;
    }
    catch (const RvException &e) {
        std::cerr << "We encountered an exception " << typeid(e).name() << ", " << e.what() << std::endl;
    }
    return inst_exec;
}

uint64_t RvMultiCycleCpu::get_cycle_count() const
{
    return executed_cycles;
}

double RvMultiCycleCpu::get_cpi() const
{
    return static_cast<double>(executed_cycles) / executed_insts;
}

const std::unordered_map<std::string, uint64_t> &RvMultiCycleCpu::get_inst_stat() const
{
    return inst_stat;
}

void RvMultiCycleCpu::reset_stat()
{
    executed_cycles = 0;
    executed_insts = 0;
    inst_stat.clear();
}

#pragma endregion


#pragma region RvPipelineCpu

void RvPipelineCpu::stage_fetch()
{
    bool should_stall{ fetch_cycle > 0 };
    RvInst *inst{};
    try {
        inst = RvInst::decode(mem.fetch(fetch_pc));
    }
    catch (const RvAccVio &) {
        inst = new RvMemFInst;
        should_stall = true;
        fetch_cycle = 1;
    }
    catch (const RvIllIns &) {
        inst = new RvIllFInst;
        should_stall = true;
        fetch_cycle = 1;
    }
    catch (const RvMisAlign &) {
        inst = new RvMemFInst;
        should_stall = true;
        fetch_cycle = 1;
    }
    fetch_cycle = std::max<uint64_t>(mem.mem_cycle() - 1, fetch_cycle);
    fetch_inst.reset(inst);
    fetch_reg.pc = fetch_pc;
    if (!should_stall) {
        if (auto tmp_ptr{ dynamic_cast<RvSBInst *>(inst) })
            fetch_pc = predictor->pred(fetch_pc, tmp_ptr->get_target(fetch_pc)) ? tmp_ptr->get_target(fetch_pc) : fetch_pc + 4;
        else
            fetch_pc += 4;
    }
    else
        fetch_cycle--;
}

void RvPipelineCpu::stage_decode()
{
    auto tmp_pc{ decode_reg.pc };
    //decode_reg = reg;
    decode_reg.pc = tmp_pc;
    decode_cycle--;
}

void RvPipelineCpu::stage_exec()
{
    if (exec_inst && exec_inst->data_hazard(decode_inst.get()) == RvInst::H_RAW) {
        decode_cycle = 2;
        fetch_cycle = std::max<uint64_t>(fetch_cycle, 1);
        has_raw = true;
    }
    if (exec_cycle >= 1) {
        exec_cycle--;
        if (exec_cycle) {
            decode_cycle = 2;
            fetch_cycle = std::max<uint64_t>(fetch_cycle, 1);
        }
        return;
    }
    if (!exec_inst) {
        return;
    }
    if (dynamic_cast<RvFaultInst *>(exec_inst.get()))
        return;
    try {
        exec_inst->exec(exec_reg);
        exec_cycle = exec_inst->exec_cycle() - 1;
        if (exec_cycle > 0) {
            decode_cycle = 2;
            fetch_cycle = std::max<uint64_t>(fetch_cycle, 1);
        }
    }
    catch (const RvIllIns &) {
        exec_inst.reset(new RvIllFInst);
    }
    catch (const RvMemAcc &info) {
        mem_acc_info = info;
    }
    catch (const RvCtrlFlowJmp &info) {
        exec_cycle = 0;
        throw;
    }
}

void RvPipelineCpu::stage_mem()
{
    if (mem_inst && mem_inst->data_hazard(decode_inst.get()) == RvInst::H_RAW) {
        decode_cycle = 2;
        fetch_cycle = std::max<uint64_t>(fetch_cycle, 1);
        has_raw = true;
    }
    if (mem_cycle >= 1) {
        mem_cycle--;
        if (mem_cycle) {
            decode_cycle = 2;
            fetch_cycle = std::max<uint64_t>(fetch_cycle, 1);
        }
        return;
    }
    if (!mem_inst)
        return;
    if (!mem_acc_info)
        return;
    
    try {
        mem_inst->mem(mem_reg, mem, *mem_acc_info);
        mem_cycle = mem.mem_cycle() - 1;
        if (mem_cycle > 0) {
            decode_cycle = 2;
            fetch_cycle = std::max<uint64_t>(fetch_cycle, 1);
        }
        mem_acc_info = std::nullopt;
    }
    catch(RvAccVio &) {
        mem_inst.reset(new RvMemFInst);
        mem_acc_info = std::nullopt;
    }
}

void RvPipelineCpu::stage_wb()
{
    if (wb_inst && wb_inst->data_hazard(decode_inst.get()) == RvInst::H_RAW) {
        decode_cycle = 2;
        fetch_cycle = std::max<uint64_t>(fetch_cycle, 1);
        has_raw = true;
    }
    if (!wb_inst)
        return;
    try {
        //reg = wb_reg;
        wb_inst->write_back(wb_reg, reg);
        executed_insts++;
        inst_stat[wb_inst->inst_name()]++;
        std::cout << wb_inst->name() << std::endl;
    }
    catch (const RvException &) {
        fetch_pc = wb_reg.pc;
        throw RvHalt{};
    }
}

RvPipelineCpu::RvPipelineCpu(RvMem &mem, const RvReg &reg, std::shared_ptr<RvBranchPred> branch_pred)
    : RvBaseCpu(mem, reg)
    , executed_cycles{}
    , executed_insts{}
    , branch_insts{}
    , branch_miss{}
    , squashed_insts{}
    , raw_stall_cycles{}
    , inst_stat{}
    , fetch_pc{ reg.pc }
    , predictor{ branch_pred }
    , fetch_cycle{}
    , decode_cycle{}
    , exec_cycle{}
    , mem_cycle{}
    , fetch_invd{ false }
    , decode_invd{ false }
    , exec_invd{ false }
    , mem_invd{ false }
    , wb_invd{ false }
    , has_raw{ false }
{
    wb_reg = reg;
    return;
}

void RvPipelineCpu::step() try
{
    wb_inst.reset();
    if (mem_cycle == 0) {
        mem_inst.swap(wb_inst);
        wb_reg = mem_reg;
    }
    if (!mem_inst && exec_cycle == 0) {
        exec_inst.swap(mem_inst);
        mem_reg = exec_reg;
    }
    if (!exec_inst && decode_cycle == 0) {
        decode_inst.swap(exec_inst);
        exec_reg = decode_reg;
    }
    auto tmp_decode_pc = decode_reg.pc;
    decode_reg = reg;
    decode_reg.pc = tmp_decode_pc;
    if (!decode_inst && fetch_cycle == 0) {
        fetch_inst.swap(decode_inst);
        decode_reg.pc = fetch_reg.pc;
    }
    //fetch_reg = reg;
    if (exec_invd) {
        exec_cycle = 0;
        exec_inst.reset();
        exec_invd = false;
    }
    if (decode_invd) {
        decode_cycle = 0;
        decode_inst.reset();
        decode_invd = false;
    }

    executed_cycles++;
    decode_cycle = std::max<uint64_t>(1, decode_cycle);
    stage_wb();
    stage_mem();
    if (auto tmp_ptr{ dynamic_cast<RvSBInst *>(exec_inst.get()) }) {
        bool taken{ false };
        uint64_t real_pc{ exec_reg.pc + 4 };
        branch_insts++;
        try {
            stage_exec();
        }
        catch (const RvCtrlFlowJmp &info) {
            real_pc = info.target_addr;
            taken = true;
        }
        predictor->update(exec_reg.pc, taken);
        if ((decode_inst && decode_reg.pc != real_pc) || (!decode_inst && fetch_pc != real_pc)) {
            if (decode_inst)
                squashed_insts++;
            decode_invd = true;
            exec_invd = true;
            squashed_insts++;
            fetch_pc = real_pc;
            fetch_cycle = 1;
            decode_cycle = 2;
            branch_miss++;
        }
    }
    else {
        try {
            stage_exec();
        }
        catch (const RvCtrlFlowJmp &info) {
            if (decode_inst)
                squashed_insts++;
            decode_invd = true;
            exec_invd = true;
            squashed_insts++;
            fetch_pc = info.target_addr;
            fetch_cycle = 1;
            decode_cycle = 2;
        }
    }
    stage_decode();
    stage_fetch();

    raw_stall_cycles += has_raw;
    has_raw = false;

    return;
}
catch (const RvHalt &)
{
    fetch_reg = decode_reg = exec_reg = mem_reg = wb_reg = RvReg{};
    fetch_inst.reset();
    decode_inst.reset();
    decode_cycle = 1;
    exec_inst.reset();
    mem_inst.reset();
    wb_inst.reset();
    has_raw = false;
    throw;
}

uint64_t RvPipelineCpu::exec(uint64_t cycle, bool no_bp)
{
    uint64_t last_executed{ executed_insts };
    try {
        if (cycle != 0) {
            while (cycle--) {
                if (no_bp || breakpoint.find(reg.pc) == breakpoint.end())
                    step();
                else
                    break;
            }
        }
        else {
            for (;;) {
                if (no_bp || breakpoint.find(reg.pc) == breakpoint.end())
                    step();
                else
                    break;
            }
        }
    }
    catch (const RvHalt &) {
        ;
    }
    catch (const RvException &) {
        ;
    }
    return executed_insts - last_executed;
}

double RvPipelineCpu::get_missrate() const
{
    return static_cast<double>(branch_miss) / branch_insts;
}

double RvPipelineCpu::get_cpi() const
{
    return static_cast<double>(executed_cycles) / executed_insts;
}

uint64_t RvPipelineCpu::get_cycle_count() const
{
    return executed_cycles;
}

uint64_t RvPipelineCpu::get_inst_count() const
{
    return executed_insts;
}

uint64_t RvPipelineCpu::get_branch_count() const
{
    return branch_insts;
}

uint64_t RvPipelineCpu::get_branch_miss() const
{
    return branch_miss;
}

uint64_t RvPipelineCpu::get_squashed_inst_count() const
{
    return squashed_insts;
}

uint64_t RvPipelineCpu::get_raw_stall_count() const
{
    return raw_stall_cycles;
}

uint64_t RvPipelineCpu::get_fetch_pc() const
{
    return fetch_pc;
}

const decltype(RvPipelineCpu::inst_stat) &RvPipelineCpu::get_inst_stat() const
{
    return inst_stat;
}

RvPipelineCpu::status_t RvPipelineCpu::get_internal_status() const
{
    return {
        fetch_inst ? fetch_inst->name() : "nop"s,
        decode_inst ? decode_inst->name() : "nop"s,
        exec_inst ? exec_inst->name() : "nop"s,
        mem_inst ? mem_inst->name() : "nop"s,
        wb_inst ? wb_inst->name() : "nop"s,
        fetch_cycle,
        decode_cycle,
        exec_cycle,
        mem_cycle,
        0
    };
}

void RvPipelineCpu::reset_stat()
{
    inst_stat.clear();
    executed_insts = 0;
    executed_cycles = 0;
    branch_insts = 0;
    branch_miss = 0;
    squashed_insts = 0;
    raw_stall_cycles = 0;
}

#pragma endregion
