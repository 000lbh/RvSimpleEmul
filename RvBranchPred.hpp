#pragma once

#include <cstdint>
#include <memory>

class RvBranchPred {
public:
    virtual bool pred(uint64_t pc, uint64_t target) = 0;
    virtual void update(uint64_t pc, bool taken) = 0;
};

template <bool _jump>
class RvStaticBranchPred : public RvBranchPred {
public:
    bool pred(uint64_t pc, uint64_t target) override { return _jump; }
    void update(uint64_t pc, bool taken) override {};
};

// Backward taken, forward not taken
class RvStaticBTFNTBranchPred : public RvBranchPred {
public:
    bool pred(uint64_t pc, uint64_t target) override { return pc > target; }
    void update(uint64_t pc, bool taken) override {};
};

template <uint8_t _counter_len = 2>
class RvSatCtrPred : public RvBranchPred {
    // PC is always aligned to 2, so actual mask bit length is pc_len + 1;
    uint8_t pc_len;
    class SatCounter {
        uint8_t data;
        constexpr static uint8_t max_data{ (1 << _counter_len) - 1 };
    public:
        SatCounter(uint8_t data = 0) : data{ data & max_data } {}
        SatCounter &operator++()
        {
            if (data != max_data)
                data++;
            return *this;
        }
        void operator++(int)
        {
            ++*this;
        }
        SatCounter &operator--()
        {
            if (data != 0)
                data--;
            return *this;
        }
        void operator--(int)
        {
            --*this;
        }
        bool above_half() { return data > max_data / 2; }
    };
    std::unique_ptr<SatCounter[]> counters;
public:
    RvSatCtrPred(uint8_t pc_len = 12)
        : pc_len{ pc_len }
        , counters{ new SatCounter[1 << pc_len] }
    {
        return;
    }
    bool pred(uint64_t pc, uint64_t target) override
    {
        pc = (pc >> 1) & ((1 << pc_len) - 1);
        return counters[pc].above_half();
    }
    void update(uint64_t pc, bool taken) override
    {
        pc = (pc >> 1) & ((1 << pc_len) - 1);
        if (taken)
            counters[pc]++;
        else
            counters[pc]--;
    }
};
