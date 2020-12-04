#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>
#include <assert.h>

enum MemorySlot
{
    kNone,          // e.g. regs
    kDisasm,        // general disassembly view memory
    kDisasmPC,      // auto-requested memory at the stopped PC
    kMemview,       // general memory view memory

    kMemorySlotCount
};

// A block of memory pulled from the target.
struct Memory
{
public:
    Memory(uint32_t addr, uint32_t size) :
        m_addr(addr),
        m_size(size)
    {
        m_pData = new uint8_t[size];
    }

    ~Memory()
    {
        delete [] m_pData;
    }

    void Set(uint32_t offset, uint8_t val)
    {
        assert(offset < m_size);
        m_pData[offset] = val;
    }

    uint8_t Get(uint32_t offset) const
    {
        assert(offset < m_size);
        return m_pData[offset];
    }

    uint32_t GetAddress() const
    {
        return m_addr;
    }

    uint32_t GetSize() const
    {
        return m_size;
    }

    const uint8_t* GetData() const
    {
        return m_pData;
    }

private:
    Memory(const Memory& other);	// hide to prevent accidental use

    uint32_t	m_addr;
    uint32_t	m_size;		// size in bytes
    uint8_t*	m_pData;
};

#endif // MEMORY_H
