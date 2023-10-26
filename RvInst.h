#pragma once

#include <deque>
#include <string>
#include <cstdint>
#include <optional>

class RvInst;

#include "RvCpu.h"
#include "RvMem.h"
#include "RvExcept.hpp"

class RvInst {
protected:
    uint8_t opcode;
    RvInst() = default;
    RvInst(const RvInst &) = default;
public:
    enum hazard_t {
        H_RAW = 0,
        H_NOHAZARD = 1,
        H_WAR = 2,
        H_WAW = 3,
    };
    static RvInst *decode(uint32_t inst = 0);

    virtual std::string name() const = 0;
    virtual std::string inst_name() const = 0;
    virtual void exec(RvReg &reg) const = 0;
    virtual void mem(RvReg &reg, RvMem &mem, const RvMemAcc &info) const;
    virtual void write_back(RvReg &src, RvReg &dest) const = 0;
    virtual RvInst *copy() const = 0;
    virtual hazard_t data_hazard(RvInst *subsequent_inst);
    virtual bool div_rem_ok(RvInst *subsequent_inst);
    virtual uint64_t exec_cycle();
};

class RvRInst : public RvInst {
    friend class RvInst;
protected:
    uint8_t funct3;
    uint8_t funct7;
    uint8_t rs1;
    uint8_t rs2;
    uint8_t rd;
    RvRInst(uint32_t inst);
    RvRInst(const RvRInst &) = default;
public:
    std::string name() const override;
    std::string inst_name() const override;
    void exec(RvReg &reg) const override;
    void write_back(RvReg &src, RvReg &dest) const override;
    RvInst *copy() const override;
    bool div_rem_ok(RvInst *subsequent_inst) override;
    uint64_t exec_cycle() override;
};

class RvIInst : public RvInst {
    friend class RvInst;
protected:
    uint8_t funct3;
    int64_t imm;
    uint8_t rs1;
    uint8_t rd;
    std::optional<uint8_t> funct7;
    RvIInst(uint32_t inst);
    RvIInst(const RvIInst &) = default;
public:
    std::string name() const override;
    std::string inst_name() const override;
    void exec(RvReg &reg) const override;
    void mem(RvReg &reg, RvMem &mem, const RvMemAcc &info) const override;
    void write_back(RvReg &src, RvReg &dest) const override;
    RvInst *copy() const override;
};

class RvSInst : public RvInst {
    friend class RvInst;
protected:
    uint8_t funct3;
    int64_t imm;
    uint8_t rs1;
    uint8_t rs2;
    RvSInst(uint32_t inst);
    RvSInst(const RvSInst &) = default;
public:
    std::string name() const override;
    std::string inst_name() const override;
    void exec(RvReg &reg) const override;
    void mem(RvReg &reg, RvMem &mem, const RvMemAcc &info) const override;
    void write_back(RvReg &src, RvReg &dest) const override;
    RvInst *copy() const override;
};

class RvSBInst : public RvInst {
    friend class RvInst;
protected:
    uint8_t funct3;
    int64_t imm;
    uint8_t rs1;
    uint8_t rs2;
    RvSBInst(uint32_t inst);
    RvSBInst(const RvSBInst &) = default;
public:
    std::string name() const override;
    std::string inst_name() const override;
    void exec(RvReg &reg) const override;
    void write_back(RvReg &src, RvReg &dest) const override;
    RvInst *copy() const override;
    uint64_t get_target(uint64_t pc) const;
};

class RvUInst : public RvInst {
    friend class RvInst;
protected:
    int64_t imm;
    uint8_t rd;
    RvUInst(uint32_t inst);
    RvUInst(const RvUInst &) = default;
public:
    std::string name() const override;
    std::string inst_name() const override;
    void exec(RvReg &reg) const override;
    void write_back(RvReg &src, RvReg &dest) const override;
    RvInst *copy() const override;
};

class RvUJInst : public RvInst {
    friend class RvInst;
protected:
    int64_t imm;
    uint8_t rd;
    RvUJInst(uint32_t inst);
    RvUJInst(const RvUJInst &) = default;
public:
    std::string name() const override;
    std::string inst_name() const override;
    void exec(RvReg &reg) const override;
    void write_back(RvReg &src, RvReg &dest) const override;
    RvInst *copy() const override;
};

class RvFaultInst : public RvInst {
    friend class RvInst;
protected:
    RvFaultInst(const RvFaultInst &) = default;
public:
    RvFaultInst() = default;
    std::string name() const override = 0;
    std::string inst_name() const override = 0;
    void exec(RvReg &reg) const override;
    void mem(RvReg &reg, RvMem &mem, const RvMemAcc &meminfo) const override;
    void write_back(RvReg &src, RvReg &dest) const override;
    RvInst *copy() const override = 0;
};

class RvIllFInst : public RvFaultInst {
    friend class RvInst;
protected:
    RvIllFInst(const RvIllFInst &) = default;
public:
    RvIllFInst() = default;
    std::string name() const override;
    std::string inst_name() const override;
    RvInst *copy() const override;
};

class RvMemFInst : public RvFaultInst {
    friend class RvInst;
protected:
    RvMemFInst(const RvMemFInst &) = default;
public:
    RvMemFInst() = default;
    std::string name() const override;
    std::string inst_name() const override;
    RvInst *copy() const override;
};
