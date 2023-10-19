#pragma once

#include <exception>
#include <stdexcept>
#include <string>

class RvException : public std::exception {
protected:
    uint64_t addr;
    std::string reason;
    RvException(uint64_t addr, std::string reason = "") : addr(addr), reason(reason) {}
public:
    virtual const char *what() const noexcept override { return reason.c_str(); }
    virtual uint64_t fault_addr() const { return addr; }
};

class RvIllIns : public RvException {
public:
    RvIllIns(uint64_t addr, std::string reason = "Illegal Instruction") : RvException(addr, reason) {}
};

class RvAccVio : public RvException {
    uint64_t type;
public:
    RvAccVio(uint64_t addr, std::string reason = "Access Violation") : RvException(addr, reason) {}
};

class RvMisAlign : public RvException {
public:
    RvMisAlign(uint64_t addr, std::string reason = "Misaligned") : RvException(addr, reason) {}
};

class RvHalt : public RvException {
public:
    RvHalt() : RvException(0, "") {}
};

class RvSysCall : public RvException {
public:
    RvSysCall() : RvException(0, "") {}
};

struct RvCtrlFlowJmp {
    uint64_t target_addr;
};

struct RvMemAcc {
    uint64_t target_addr;
    size_t width;
    bool sign;
    enum rw_t { READ, WRITE } rw;
};
