#include "memory.h"

#include <string.h>

Memory::Memory(uint32_t addr, uint32_t size) :
    m_addr(addr),
    m_size(size)
{
    m_pData = new uint8_t[size];
}

Memory::~Memory()
{
    delete [] m_pData;
}

Memory& Memory::operator=(const Memory &other)
{
    this->m_size = other.m_size;
    this->m_addr = other.m_addr;
    this->m_pData = new uint8_t[other.m_size];
    memcpy(this->m_pData, other.m_pData, other.m_size);
    return *this;
}
