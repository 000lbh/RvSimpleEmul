#include "RvInst.h"

#include <unordered_map>
#include <functional>
#include <bit>

#include "RvExcept.hpp"

using namespace std::string_literals;

constexpr uint8_t get_opcode(uint32_t inst) {
    return inst & 0b1111111;
}
constexpr uint8_t get_rd(uint32_t inst) {
    return (inst & 0b111110000000) >> 7;
}
constexpr uint8_t get_funct3(uint32_t inst) {
    return (inst & 0x7000) >> 12;
}
constexpr uint8_t get_rs1(uint32_t inst) {
    return (inst & 0xf8000) >> 15;
}
constexpr uint8_t get_rs2(uint32_t inst) {
    return (inst & 0x1f00000) >> 20;
}
constexpr uint8_t get_funct7(uint32_t inst) {
    return inst >> 25;
}
constexpr int64_t get_i_imm(uint32_t inst) {
    return (static_cast<int64_t>(inst) << 32) >> 52;
}
constexpr int64_t get_s_imm(uint32_t inst) {
    int64_t result = (static_cast<int64_t>(inst) << 32) >> 57;
    return (result << 5) | ((inst & 0b111110000000) >> 7);
}
constexpr int64_t get_sb_imm(uint32_t inst) {
    bool sign = static_cast<bool>(inst & 0x80000000);
    int64_t result = sign ? ~0xfff : 0;
    return result | ((inst & 0x7E000000) >> 20) | ((inst & 0x00000F00) >> 7) | ((inst & 0x00000080) << 4);
}
constexpr int64_t get_u_imm(uint32_t inst) {
    return (static_cast<int64_t>(inst & 0xfffff000) << 32) >> 32;
}
constexpr int64_t get_uj_imm(uint32_t inst) {
    bool sign = static_cast<bool>(inst & 0x80000000);
    int64_t result = sign ? ~0xfffff : 0;
    return result | ((inst & 0x7fe00000) >> 20) | ((inst & 0x00100000) >> 9) | (inst & 0x0000ff000);
}

RvInst *RvInst::decode(uint32_t inst) {
    switch (inst & 0b1111111) {
    case 0x33:
        // R-type 64-bit arithmetic
    case 0x3b:
        // R-type 32-bit arithmetic
        return new RvRInst(inst);
    case 0x03:
        // I-type load insts
    case 0x13:
        // I-type 64-bit immediate arithmetic
    case 0x1b:
        // I-type 32-bit immediate arithmetic
    case 0x67:
        // I-type jump and link register
    case 0x73:
        // I-type transfer control to kernel
        return new RvIInst(inst);
    case 0x23:
        // S-type store insts
        return new RvSInst(inst);
    case 0x63:
        // SB-type branch insts
        return new RvSBInst(inst);
    case 0x17:
        // U-type add upper immediate to PC
    case 0x37:
        // U-type load upper immediate
        return new RvUInst(inst);
    case 0x6f:
        // UJ-type jump far
        return new RvUJInst(inst);
    default:
        return new RvIllFInst{};
    }
}

void RvInst::mem(RvReg &reg, RvMem &mem, const RvMemAcc &info) const
{
    return;
}

RvInst::hazard_t RvInst::data_hazard(RvInst *subsequent_inst)
{
    if (!subsequent_inst)
        return H_NOHAZARD;
    /*
    RvReg tmp_reg{};
    std::optional<RvMemAcc> this_inst;
    std::optional<RvMemAcc> subs_inst;
    try {
        this->exec(tmp_reg);
    }
    catch (const RvMemAcc &meminfo) {
        this_inst = meminfo;
    }
    catch (const RvCtrlFlowJmp &) {
        ;
    }
    catch (const RvException &) {
        return H_NOHAZARD;
    }
    try {
        subsequent_inst->exec(tmp_reg);
    }
    catch (const RvMemAcc &meminfo) {
        subs_inst = meminfo;
    }
    catch (const RvCtrlFlowJmp &) {
        ;
    }
    catch (const RvException &) {
        return H_NOHAZARD;
    }
    if (this_inst && subs_inst && this_inst->target_addr == subs_inst->target_addr) {
        if (this_inst->rw == RvMemAcc::WRITE && subs_inst->rw == RvMemAcc::READ) {
            return H_RAW;
        }
        else if (this_inst->rw == RvMemAcc::READ && subs_inst->rw == RvMemAcc::WRITE) {
            return H_WAR;
        }
        else if (this_inst->rw == RvMemAcc::WRITE && subs_inst->rw == RvMemAcc::WRITE) {
            return H_WAW;
        }
    }
    */

    std::optional<uint8_t> this_rs1;
    std::optional<uint8_t> this_rs2;
    std::optional<uint8_t> this_rd;
    std::optional<uint8_t> subs_rs1;
    std::optional<uint8_t> subs_rs2;
    std::optional<uint8_t> subs_rd;

    // Get this regs
    if (auto ptr{ dynamic_cast<RvRInst *>(this) }) {
        this_rs1 = ptr->rs1;
        this_rs2 = ptr->rs2;
        if (ptr->rd)
            this_rd = ptr->rd;
    }
    else if (auto ptr{ dynamic_cast<RvIInst *>(this) }) {
        this_rs1 = ptr->rs1;
        if (ptr->rd)
            this_rd = ptr->rd;
    }
    else if (auto ptr{ dynamic_cast<RvSInst *>(this) }) {
        this_rs1 = ptr->rs1;
        this_rs2 = ptr->rs2;
    }
    else if (auto ptr{ dynamic_cast<RvSBInst *>(this) }) {
        this_rs1 = ptr->rs1;
        this_rs2 = ptr->rs2;
    }
    else if (auto ptr{ dynamic_cast<RvUInst *>(this) }) {
        if (ptr->rd)
            this_rd = ptr->rd;
    }
    else if (auto ptr{ dynamic_cast<RvUJInst *>(this) }) {
        if (ptr->rd)
            this_rd = ptr->rd;
    }

    // Get subsequent inst regs
    if (auto ptr{ dynamic_cast<RvRInst *>(subsequent_inst) }) {
        subs_rs1 = ptr->rs1;
        subs_rs2 = ptr->rs2;
        if (ptr->rd)
            subs_rd = ptr->rd;
    }
    else if (auto ptr{ dynamic_cast<RvIInst *>(subsequent_inst) }) {
        subs_rs1 = ptr->rs1;
        if (ptr->rd)
            subs_rd = ptr->rd;
    }
    else if (auto ptr{ dynamic_cast<RvSInst *>(subsequent_inst) }) {
        subs_rs1 = ptr->rs1;
        subs_rs2 = ptr->rs2;
    }
    else if (auto ptr{ dynamic_cast<RvSBInst *>(subsequent_inst) }) {
        subs_rs1 = ptr->rs1;
        subs_rs2 = ptr->rs2;
    }
    else if (auto ptr{ dynamic_cast<RvUInst *>(subsequent_inst) }) {
        if (ptr->rd)
            subs_rd = ptr->rd;
    }
    else if (auto ptr{ dynamic_cast<RvUJInst *>(subsequent_inst) }) {
        if (ptr->rd)
            subs_rd = ptr->rd;
    }
    if (this_rd && (this_rd == subs_rs1 || this_rd == subs_rs2))
        return H_RAW;
    else if ((this_rs1 && this_rs1 == subs_rd) || (this_rs2 && this_rs2 == subs_rd))
        return H_WAR;
    else if (this_rd && this_rd == subs_rd)
        return H_WAW;
    return H_NOHAZARD;
}

bool RvInst::div_rem_ok(RvInst *subsequent_inst) {
    return false;
}

uint64_t RvInst::exec_cycle()
{
    return 1;
}

#pragma region RvRInst

std::unordered_map<uint8_t, std::unordered_map<uint8_t, std::unordered_map<uint8_t, std::string>>> RvRInstNameDict{
    { 0x33, {
        { 0x00, {
            { 0x00, "add" },
            { 0x01, "mul" },
            { 0x20, "sub" }
        } },
        { 0x01, {
            { 0x00, "sll" },
            { 0x01, "mulh" }
        } },
        { 0x02, {
            { 0x00, "slt" },
        } },
        { 0x03, {
            { 0x00, "sltu" },
        } },
        { 0x04, {
            { 0x00, "xor" },
            { 0x01, "div" }
        } },
        { 0x05, {
            { 0x00, "srl" },
            { 0x01, "divu" },
            { 0x20, "sra" }
        } },
        { 0x06, {
            { 0x00, "or" },
            { 0x01, "rem" },
        } },
        { 0x07, {
            { 0x00, "and" },
            { 0x01, "remu" }
        } }
    } },
    { 0x3b, {
        {0x00, {
            { 0x00, "addw" },
            { 0x01, "mulw" },
            { 0x20, "subw" }
        } },
        {0x04, {
            { 0x01, "divw" }
        } },
        {0x06, {
            { 0x01, "remw" }
        } }
    } }
};

std::unordered_map<uint8_t, std::unordered_map<uint8_t, std::unordered_map<uint8_t, uint64_t>>> RvRInstCycleDict{
    { 0x33, {
        { 0x00, {
            { 0x00, 1 },
            { 0x01, 2 },
            { 0x20, 1 }
        } },
        { 0x01, {
            { 0x00, 1 },
            { 0x01, 2 }
        } },
        { 0x02, {
            { 0x00, 1 },
        } },
        { 0x03, {
            { 0x00, 1 },
        } },
        { 0x04, {
            { 0x00, 1 },
            { 0x01, 40 }
        } },
        { 0x05, {
            { 0x00, 1 },
            { 0x01, 40 },
            { 0x20, 1 }
        } },
        { 0x06, {
            { 0x00, 1 },
            { 0x01, 40 },
        } },
        { 0x07, {
            { 0x00, 1 },
            { 0x01, 40 }
        } }
    } },
    { 0x3b, {
        {0x00, {
            { 0x00, 1 },
            { 0x01, 1 },
            { 0x20, 1 }
        } },
        {0x04, {
            { 0x01, 40 }
        } },
        {0x06, {
            { 0x01, 40 }
        } }
    } }
};

#ifdef _MSC_VER
#include <__msvc_int128.hpp>
uint64_t imull(uint64_t s1, uint64_t s2) {
    std::_Signed128 result = s1;
    result *= s2;
    return static_cast<uint64_t>(result);
}
uint64_t imulh(uint64_t s1, uint64_t s2) {
    std::_Signed128 result = s1;
    result *= s2;
    result >>= 64;
    return static_cast<uint64_t>(result);
}
#endif

#ifdef __GNUC__
uint64_t imull(uint64_t s1, uint64_t s2) {
    __int128_t result = s1;
    result *= s2;
    return result;
}
uint64_t imulh(uint64_t s1, uint64_t s2) {
    __int128_t result = s1;
    result *= s2;
    return result >> 64;
}
#endif

std::unordered_map<uint8_t, std::unordered_map<uint8_t, std::unordered_map<uint8_t, std::function<uint64_t(uint64_t, uint64_t)>>>> RvRInstExecDict{
    { 0x33, {
        { 0x00, {
            { 0x00, [](uint64_t s1, uint64_t s2) { return s1 + s2; } },
            { 0x01, imull },
            { 0x20, [](uint64_t s1, uint64_t s2) { return s1 - s2; } }
        } },
        { 0x01, {
            { 0x00, [](uint64_t s1, uint64_t s2) { return s1 << s2; } },
            { 0x01, imulh }
        } },
        { 0x02, {
            { 0x00, [](uint64_t s1, uint64_t s2) -> uint64_t { return std::bit_cast<int64_t>(s1) < std::bit_cast<int64_t>(s2) ? 1 : 0; } },
        } },
        { 0x03, {
            { 0x00, [](uint64_t s1, uint64_t s2) -> uint64_t { return s1 < s2 ? 1 : 0; } },
        } },
        { 0x04, {
            { 0x00, [](uint64_t s1, uint64_t s2) { return s1 ^ s2; } },
            { 0x01, [](uint64_t s1, uint64_t s2) -> uint64_t { return std::bit_cast<int64_t>(s1) / std::bit_cast<int64_t>(s2); } }
        } },
        { 0x05, {
            { 0x00, [](uint64_t s1, uint64_t s2) { return s1 >> s2; } },
            { 0x01, [](uint64_t s1, uint64_t s2) { return s1 / s2; } },
            { 0x20, [](uint64_t s1, uint64_t s2) -> uint64_t { return std::bit_cast<int64_t>(s1) >> std::bit_cast<int64_t>(s2); } }
        } },
        { 0x06, {
            { 0x00, [](uint64_t s1, uint64_t s2) { return s1 | s2; } },
            { 0x01, [](uint64_t s1, uint64_t s2) -> uint64_t { return std::bit_cast<int64_t>(s1) % std::bit_cast<int64_t>(s2); } },
        } },
        { 0x07, {
            { 0x00, [](uint64_t s1, uint64_t s2) { return s1 & s2; } },
            { 0x01, [](uint64_t s1, uint64_t s2) { return s1 % s2; } }
        } }
    } },
    { 0x3b, {
        {0x00, {
            { 0x00, [](uint64_t s1, uint64_t s2) -> uint64_t { return static_cast<int64_t>(static_cast<int32_t>(s1 + s2)); } },
            { 0x01, [](uint64_t s1, uint64_t s2) -> uint64_t { return static_cast<int64_t>(static_cast<int32_t>(s1) * static_cast<int32_t>(s2)); } },
            { 0x20, [](uint64_t s1, uint64_t s2) -> uint64_t { return static_cast<int64_t>(static_cast<int32_t>(s1 - s2)); } }
        } },
        {0x04, {
            { 0x01, [](uint64_t s1, uint64_t s2) -> uint64_t { return static_cast<int64_t>(static_cast<int32_t>(s1) / static_cast<int32_t>(s2)); } }
        } },
        {0x06, {
            { 0x01, [](uint64_t s1, uint64_t s2) -> uint64_t { return static_cast<int64_t>(static_cast<int32_t>(s1) % static_cast<int32_t>(s2)); } }
        } }
    } }
};

RvRInst::RvRInst(uint32_t inst)
    : funct3{ get_funct3(inst) }
    , funct7{ get_funct7(inst) }
    , rs1{ get_rs1(inst) }
    , rs2{ get_rs2(inst) }
    , rd{ get_rd(inst) }
{
    opcode = get_opcode(inst);
    return;
}

std::string RvRInst::name() const {
    return inst_name() + " " + RVREGABINAME[rd] + ", " + RVREGABINAME[rs1] + ", " + RVREGABINAME[rs2];
}

std::string RvRInst::inst_name() const {
    std::string result{};
    try {
        result = RvRInstNameDict.at(opcode).at(funct3).at(funct7);
    }
    catch (std::out_of_range) {
        result = "undefined"s;
    }
    return std::move(result);
}

void RvRInst::exec(RvReg &reg) const try {
    reg.set(rd, RvRInstExecDict.at(opcode).at(funct3).at(funct7)(reg[rs1], reg[rs2]));
}
catch (std::out_of_range) {
    throw RvIllIns(reg.pc);
}

void RvRInst::write_back(RvReg &src, RvReg &dest) const
{
    dest[rd] = src[rd];
}

RvInst *RvRInst::copy() const
{
    return new RvRInst{ *this };
}

bool RvRInst::div_rem_ok(RvInst *subsequent_inst)
{
    if ((this->inst_name() != "div"s || subsequent_inst->inst_name() != "rem"s) && (this->inst_name() != "divu"s || subsequent_inst->inst_name() != "remu"s))
        return false;
    auto other{ dynamic_cast<RvRInst *>(subsequent_inst) };
    return this->rs1 == other->rs1 && this->rs2 == other->rs2 && this->rd != other->rs1 && this->rd != other->rs2;
}

uint64_t RvRInst::exec_cycle()
{
    return RvRInstCycleDict.at(opcode).at(funct3).at(funct7);
}

#pragma endregion

#pragma region RvIInst

std::unordered_map<uint8_t, std::unordered_map<uint8_t, std::unordered_map<uint8_t, std::string>>> RvIInstWithF7NameDict{
    { 0x13, {
        { 0x01, {
            { 0x00, "slli" }
        } },
        { 0x05, {
            { 0x00, "srli" },
            { 0x20, "srai" }
        } },
    } }, 
    { 0x1B, {
        { 0x01, {
            { 0x00, "slliw" }
        } },
        { 0x05, {
            { 0x00, "srliw" },
            { 0x20, "sraiw" }
        } }
    } }
};

std::unordered_map<uint8_t, std::unordered_map<uint8_t, std::unordered_map<uint8_t, std::function<uint64_t(int64_t, int64_t)>>>> RvIInstWithF7ExecDict{
    { 0x13, {
        { 0x01, {
            { 0x00, [](int64_t s, int64_t imm) -> uint64_t { return s << imm; } }
        } },
        { 0x05, {
            { 0x00, [](int64_t s, int64_t imm) { return std::bit_cast<uint64_t>(s) >> imm; } },
            { 0x20, [](int64_t s, int64_t imm) -> uint64_t { return s >> imm; } }
        } },
    } },
    { 0x1B, {
        { 0x01, {
            { 0x00, [](int64_t s, int64_t imm) -> uint64_t { return static_cast<int64_t>(static_cast<int32_t>(s) << static_cast<int32_t>(imm)); } }
        } },
        { 0x05, {
            { 0x00, [](int64_t s, int64_t imm) -> uint64_t { return static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(s) >> static_cast<uint32_t>(imm))); } },
            { 0x20, [](int64_t s, int64_t imm) -> uint64_t { return static_cast<int64_t>(static_cast<int32_t>(s) >> static_cast<int32_t>(imm)); } }
        } }
    } }
};

std::unordered_map<uint8_t, std::unordered_map<uint8_t, std::string>> RvIInstNameDict{
    { 0x03, {
        { 0x00, "lb" },
        { 0x01, "lh" },
        { 0x02, "lw" },
        { 0x03, "ld" },
        { 0x04, "lbu" },
        { 0x05, "lhu" },
        { 0x06, "lwu" }
    } },
    { 0x13, {
        { 0x00, "addi" },
        { 0x02, "slti" },
        { 0x04, "xori" },
        { 0x06, "ori" },
        { 0x07, "andi" }
    } },
    { 0x1B, {
        { 0x00, "addiw" }
    } },
    { 0x67, {
        { 0x00, "jalr" }
    } },
    { 0x73, {
        { 0x00, "ecall"}
    } }
};

std::unordered_map<uint8_t, std::unordered_map<uint8_t, std::function<uint64_t(int64_t, int64_t)>>> RvIInstExecDict{
    { 0x03, {
        { 0x00, [](int64_t s, int64_t imm) -> uint64_t { throw RvMemAcc{std::bit_cast<uint64_t>(s + imm), 1, true, RvMemAcc::READ}; } },
        { 0x01, [](int64_t s, int64_t imm) -> uint64_t { throw RvMemAcc{std::bit_cast<uint64_t>(s + imm), 2, true, RvMemAcc::READ}; } },
        { 0x02, [](int64_t s, int64_t imm) -> uint64_t { throw RvMemAcc{std::bit_cast<uint64_t>(s + imm), 4, true, RvMemAcc::READ}; } },
        { 0x03, [](int64_t s, int64_t imm) -> uint64_t { throw RvMemAcc{std::bit_cast<uint64_t>(s + imm), 8, true, RvMemAcc::READ}; } },
        { 0x04, [](int64_t s, int64_t imm) -> uint64_t { throw RvMemAcc{std::bit_cast<uint64_t>(s + imm), 1, false, RvMemAcc::READ}; } },
        { 0x05, [](int64_t s, int64_t imm) -> uint64_t { throw RvMemAcc{std::bit_cast<uint64_t>(s + imm), 2, false, RvMemAcc::READ}; } },
        { 0x06, [](int64_t s, int64_t imm) -> uint64_t { throw RvMemAcc{std::bit_cast<uint64_t>(s + imm), 4, false, RvMemAcc::READ}; } }
    } },
    { 0x13, {
        { 0x00, [](int64_t s, int64_t imm) -> uint64_t { return s + imm; } },
        { 0x02, [](int64_t s, int64_t imm) -> uint64_t { return s < imm ? 1 : 0; } },
        { 0x04, [](int64_t s, int64_t imm) -> uint64_t { return s ^ imm; } },
        { 0x06, [](int64_t s, int64_t imm) -> uint64_t { return s | imm; } },
        { 0x07, [](int64_t s, int64_t imm) -> uint64_t { return s & imm; } }
    } },
    { 0x1B, {
        { 0x00, [](int64_t s, int64_t imm) -> uint64_t { return ((s + imm) << 32) >> 32; } }
    } },
    { 0x67, {
        { 0x00, [](int64_t s, int64_t imm) -> uint64_t { throw RvCtrlFlowJmp{ std::bit_cast<uint64_t>(s + imm) }; }}
    } },
    { 0x73, {
        {0x00, [](int64_t, int64_t) -> uint64_t { throw RvSysCall{}; } }
    } }
};

RvIInst::RvIInst(uint32_t inst)
    : funct3{ get_funct3(inst) }
    , rs1{ get_rs1(inst) }
    , rd{ get_rd(inst) }
{
    opcode = get_opcode(inst);
    try {
        void(RvIInstWithF7NameDict.at(opcode).at(funct3));
        funct7 = get_funct7(inst) & 0b1111110;
        imm = get_i_imm(inst) & 0b111111;
    }
    catch (std::out_of_range) {
        funct7 = std::nullopt;
        imm = get_i_imm(inst);
    }
    return;
}

std::string RvIInst::name() const
{
    return inst_name() + " " + RVREGABINAME[rd] + ", " + RVREGABINAME[rs1] + ", " + std::to_string(imm);
}

std::string RvIInst::inst_name() const
{
    std::string result{};
    try {
        if (funct7) {
            result = RvIInstWithF7NameDict.at(opcode).at(funct3).at(funct7.value());
        }
        else {
            result = RvIInstNameDict.at(opcode).at(funct3);
        }
    }
    catch (std::out_of_range) {
        result = "undefined";
    }
    return std::move(result);
}

void RvIInst::exec(RvReg &reg) const try {
    if (funct7)
        reg.set(rd, RvIInstWithF7ExecDict.at(opcode).at(funct3).at(funct7.value())(reg[rs1], imm));
    else
        reg.set(rd, RvIInstExecDict.at(opcode).at(funct3)(reg[rs1], imm));
}
catch (std::out_of_range) {
    throw RvIllIns(reg.pc);
}

void RvIInst::mem(RvReg &reg, RvMem &mem, const RvMemAcc &info) const {
    if (info.rw != info.READ)
        throw __LINE__;
    switch (info.width) {
    case 1:
        reg[rd] = static_cast<uint8_t>(mem[info.target_addr]);
        if (info.sign)
            reg[rd] = (static_cast<int64_t>(reg[rd]) << 56) >> 56;
        break;
    case 2:
        if (info.target_addr & 1)
            throw RvMisAlign(info.target_addr);
        reg[rd] = static_cast<uint16_t>(mem[info.target_addr]);
        if (info.sign)
            reg[rd] = (static_cast<int64_t>(reg[rd]) << 48) >> 48;
        break;
    case 4:
        if (info.target_addr & 0b11)
            throw RvMisAlign(info.target_addr);
        reg[rd] = static_cast<uint32_t>(mem[info.target_addr]);
        if (info.sign)
            reg[rd] = (static_cast<int64_t>(reg[rd]) << 32) >> 32;
        break;
    case 8:
        if (info.target_addr & 0b111)
            throw RvMisAlign(info.target_addr);
        reg[rd] = mem[info.target_addr];
        break;
    default:
        throw __LINE__;
    }
}

void RvIInst::write_back(RvReg &src, RvReg &dest) const
{
    dest[rd] = src[rd];
}

RvInst *RvIInst::copy() const
{
    return new RvIInst{ *this };
}

#pragma endregion

#pragma region RvSInst

RvSInst::RvSInst(uint32_t inst)
    : rs1(get_rs1(inst))
    , rs2(get_rs2(inst))
    , funct3(get_funct3(inst))
    , imm(get_s_imm(inst))
{
    opcode = get_opcode(inst);
    return;
}

std::string RvSInst::name() const
{
    return inst_name() + " " + RVREGABINAME[rs2] + ", " + std::to_string(imm) + "(" + RVREGABINAME[rs1] + ")";
}

std::string RvSInst::inst_name() const
{
    switch (funct3) {
    case 0x00:
        return "sb"s;
    case 0x01:
        return "sh"s;
    case 0x02:
        return "sw"s;
    case 0x03:
        return "sd"s;
    default:
        return "undefined"s;
    }
}

void RvSInst::exec(RvReg &reg) const
{
    switch (funct3) {
    case 0x00:
        throw RvMemAcc{ reg[rs1] + imm, 1, false, RvMemAcc::WRITE };
    case 0x01:
        throw RvMemAcc{ reg[rs1] + imm, 2, false, RvMemAcc::WRITE };
    case 0x02:
        throw RvMemAcc{ reg[rs1] + imm, 4, false, RvMemAcc::WRITE };
    case 0x03:
        throw RvMemAcc{ reg[rs1] + imm, 8, false, RvMemAcc::WRITE };
    default:
        throw RvIllIns(reg.pc);
    }
}

void RvSInst::mem(RvReg &reg, RvMem &mem, const RvMemAcc &info) const
{
    if (info.rw != info.WRITE)
        throw __LINE__;
    switch (info.width) {
    case 1:
        mem[info.target_addr] = static_cast<uint8_t>(reg[rs2]);
        break;
    case 2:
        if (info.target_addr & 1)
            throw RvMisAlign(info.target_addr);
        mem[info.target_addr] = static_cast<uint16_t>(reg[rs2]);
        break;
    case 4:
        if (info.target_addr & 0b11)
            throw RvMisAlign(info.target_addr);
        mem[info.target_addr] = static_cast<uint32_t>(reg[rs2]);
        break;
    case 8:
        if (info.target_addr & 0b111)
            throw RvMisAlign(info.target_addr);
        mem[info.target_addr] = static_cast<uint64_t>(reg[rs2]);
        break;
    default :
        throw __LINE__;
    }
}

void RvSInst::write_back(RvReg &src, RvReg &dest) const
{
    return;
}

RvInst *RvSInst::copy() const
{
    return new RvSInst{ *this };
}

#pragma endregion

#pragma region RvSBInst

std::unordered_map<uint8_t, std::unordered_map<uint8_t, std::string>> RvSBInstNameDict{
    { 0x63, {
        { 0x00, "beq" },
        { 0x01, "bne" },
        { 0x04, "blt" },
        { 0x05, "bge" },
        { 0x06, "bltu" },
        { 0x07, "bgeu" }
    } }
};

std::unordered_map<uint8_t, std::unordered_map<uint8_t, std::function<void(int64_t, int64_t, int64_t, uint64_t)>>> RvSBInstExecDict{
    { 0x63, {
        { 0x00, [](int64_t s1, int64_t s2, int64_t imm, uint64_t pc) {if (s1 == s2) throw RvCtrlFlowJmp{pc + imm}; } },
        { 0x01, [](int64_t s1, int64_t s2, int64_t imm, uint64_t pc) {if (s1 != s2) throw RvCtrlFlowJmp{pc + imm}; } },
        { 0x04, [](int64_t s1, int64_t s2, int64_t imm, uint64_t pc) {if (s1 < s2) throw RvCtrlFlowJmp{pc + imm}; } },
        { 0x05, [](int64_t s1, int64_t s2, int64_t imm, uint64_t pc) {if (s1 >= s2) throw RvCtrlFlowJmp{pc + imm}; } },
        { 0x06, [](int64_t s1, int64_t s2, int64_t imm, uint64_t pc) {if (std::bit_cast<uint64_t>(s1) < std::bit_cast<uint64_t>(s2)) throw RvCtrlFlowJmp{pc + imm}; } },
        { 0x07, [](int64_t s1, int64_t s2, int64_t imm, uint64_t pc) {if (std::bit_cast<uint64_t>(s1) >= std::bit_cast<uint64_t>(s2)) throw RvCtrlFlowJmp{pc + imm}; } }
    } }
};

RvSBInst::RvSBInst(uint32_t inst)
    : imm(get_sb_imm(inst))
    , funct3(get_funct3(inst))
    , rs1(get_rs1(inst))
    , rs2(get_rs2(inst))
{
    opcode = get_opcode(inst);
    return;
}

std::string RvSBInst::name() const
{
    return inst_name() + " " + RVREGABINAME[rs1] + ", " + RVREGABINAME[rs2] + ", " + std::to_string(imm);
}

std::string RvSBInst::inst_name() const
{
    std::string result{};
    try {
        result = RvSBInstNameDict.at(opcode).at(funct3);
    }
    catch (std::out_of_range) {
        result = "undefined"s;
    }
    return std::move(result);
}

void RvSBInst::exec(RvReg &reg) const try {
    RvSBInstExecDict.at(opcode).at(funct3)(reg[rs1], reg[rs2], imm, reg.pc);
}
catch (std::out_of_range) {
    throw RvIllIns(reg.pc);
}

void RvSBInst::write_back(RvReg &src, RvReg &dest) const
{
    return;
}

RvInst *RvSBInst::copy() const
{
    return new RvSBInst{ *this };
}

uint64_t RvSBInst::get_target(uint64_t pc) const
{
    return pc + imm;
}

#pragma endregion

#pragma region RvUInst

RvUInst::RvUInst(uint32_t inst)
    : imm(get_u_imm(inst))
    , rd(get_rd(inst))
{
    opcode = get_opcode(inst);
    return;
}

std::string RvUInst::name() const
{
    return inst_name() + " " + RVREGABINAME[rd] + ", " + std::to_string(imm);
}

std::string RvUInst::inst_name() const
{
    std::string result{};
    switch (opcode) {
    case 0x17:
        result = "auipc";
        break;
    case 0x37:
        result = "lui"s;
        break;
    default:
        result = "undefined"s;
    }
    return std::move(result);
}

void RvUInst::exec(RvReg &reg) const
{
    switch (opcode) {
    case 0x17:
        reg[rd] = reg.pc + imm;
        break;
    case 0x37:
        reg[rd] = imm;
        break;
    default:
        throw RvIllIns(reg.pc);
    }
}

void RvUInst::write_back(RvReg &src, RvReg &dest) const
{
    dest[rd] = src[rd];
}

RvInst *RvUInst::copy() const
{
    return new RvUInst{ *this };
}

#pragma endregion

#pragma region RvUJInst

RvUJInst::RvUJInst(uint32_t inst)
    : rd(get_rd(inst))
    , imm(get_uj_imm(inst))
{
    opcode = get_opcode(inst);
    return;
}

std::string RvUJInst::name() const
{
    if (opcode == 0x6f)
        return std::string{ "jal " } + RVREGABINAME[rd] + ", " + std::to_string(imm);
    else
        return "undefined";
}

std::string RvUJInst::inst_name() const
{
    return opcode == 0x6f ? "jal"s : "undefined"s;
}

void RvUJInst::exec(RvReg &reg) const
{
    if (opcode == 0x6f) {
        reg[rd] = reg.pc + 4;
        throw RvCtrlFlowJmp{ reg.pc + imm };
    }
    else
        throw RvIllIns(reg.pc);
}

void RvUJInst::write_back(RvReg &src, RvReg &dest) const
{
    dest[rd] = src[rd];
}

RvInst *RvUJInst::copy() const
{
    return new RvUJInst{ *this };
}

#pragma endregion

void RvFaultInst::exec(RvReg &reg) const
{
    throw RvIllIns(0);
}

void RvFaultInst::mem(RvReg &reg, RvMem &mem, const RvMemAcc &info) const
{
    throw RvIllIns(0);
}

void RvFaultInst::write_back(RvReg &, RvReg &) const
{
    throw RvHalt{};
}

#pragma region RvIllFInst

std::string RvIllFInst::name() const
{
    return "Undefined opcode"s;
}

std::string RvIllFInst::inst_name() const
{
    return "Undefined opcode"s;
}

RvInst *RvIllFInst::copy() const
{
    return new RvIllFInst{};
}

#pragma endregion

#pragma region RvMemFInst

std::string RvMemFInst::name() const
{
    return "Memory Access Violation"s;
}

std::string RvMemFInst::inst_name() const
{
    return "Memory Access violation"s;
}

RvInst *RvMemFInst::copy() const
{
    return new RvMemFInst{};
}

#pragma endregion
