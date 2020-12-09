#ifndef REGISTERS_H
#define REGISTERS_H

#include <stdint.h>
#include <assert.h>

struct Registers
{
    Registers();

    uint32_t GetDReg(uint32_t index) const
    {
        assert(index <= 7);
        return m_value[D0 + index];
    }
    uint32_t GetAReg(uint32_t index) const
    {
        assert(index <= 7);
        return m_value[A0 + index];
    }
    enum
    {
        D0 = 0,
        D1,
        D2,
        D3,
        D4,
        D5,
        D6,
        D7,
        A0,
        A1,
        A2,
        A3,
        A4,
        A5,
        A6,
        A7,
        PC,
        SR,
        USP,
        ISP,
        EX,			// Exception number
        REG_COUNT
    };
    uint32_t	m_value[REG_COUNT];
    // Null-terminated 1:1 array of register names
    static const char* s_names[];
};

#endif // REGISTERS_H
