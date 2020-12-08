#include "symboltable.h"

void SymbolTable::Add(const Symbol &sym)
{
    size_t index = m_symbols.size();
    m_addrLookup.insert(Pair(sym.address, index));
    m_symbols.push_back(sym);
}

bool SymbolTable::Find(uint32_t address, Symbol &result) const
{
    Map::const_iterator it = m_addrLookup.find(address);
    if (it == m_addrLookup.end())
        return false;

    result = m_symbols[it->second];
    return true;
}
