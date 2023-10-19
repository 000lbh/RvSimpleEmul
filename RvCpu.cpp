#include "RvCpu.h"

#include <memory>
#include <iostream>
#include <typeinfo>

#include "RvInst.h"

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
    : pc{ other.pc }
    , ra{ reg_u[1] }, sp{ reg_u[2] }, gp{ reg_u[3] }
    , tp{ reg_u[4] }, t0{ reg_u[5] }, t1{ reg_u[6] }
    , t2{ reg_u[7] }, s0{ reg_u[8] }, fp{ reg_u[8] }
    , s1{ reg_u[9] }, a0{ reg_u[10] }, a1{ reg_u[11] }
{
    std::copy(std::cbegin(other.reg_u), std::cend(other.reg_u), std::begin(reg_u));
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
    int inst_exec{};
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

