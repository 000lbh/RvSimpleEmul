#pragma once

#include <deque>
#include <string>
#include <cstdint>
#include <optional>

#include "RvCpu.h"
#include "RvMem.h"
#include "RvExcept.hpp"

class RvInst {
protected:
    uint8_t opcode;
    RvInst() = default;
    RvInst(const RvInst &) = default;
public:
    static RvInst *decode(uint32_t inst = 0);

    virtual std::string name() const = 0;
    virtual void exec(RvReg &reg) const = 0;
    virtual void mem(RvReg &reg, RvMem &mem, const RvMemAcc &info) const;
    virtual RvInst *copy() const = 0;
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
    void exec(RvReg &reg) const override;
    RvInst *copy() const override;
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
    void exec(RvReg &reg) const override;
    void mem(RvReg &reg, RvMem &mem, const RvMemAcc &info) const override;
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
    void exec(RvReg &reg) const override;
    void mem(RvReg &reg, RvMem &mem, const RvMemAcc &info) const override;
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
    void exec(RvReg &reg) const override;
    RvInst *copy() const override;
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
    void exec(RvReg &reg) const override;
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
    void exec(RvReg &reg) const override;
    RvInst *copy() const override;
};
