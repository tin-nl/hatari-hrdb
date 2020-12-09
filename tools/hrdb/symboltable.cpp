#include "symboltable.h"

void SymbolTable::Add(const Symbol &sym)
{
    size_t index = m_symbols.size();
    m_addrLookup.insert(Pair(sym.address, index));
    m_symbols.push_back(sym);

    // Recalc the keys table
    m_addrKeys.clear();
    Map::iterator it(m_addrLookup.begin());
    while (it != m_addrLookup.end())
    {
        m_addrKeys.push_back(*it);
        ++it;
    }
}

bool SymbolTable::Find(uint32_t address, Symbol &result) const
{
    Map::const_iterator it = m_addrLookup.find(address);
    if (it == m_addrLookup.end())
        return false;

    result = m_symbols[it->second];
    return true;
}

bool SymbolTable::FindLowerOrEqual(uint32_t address, Symbol &result) const
{
    // Degenerate cases
    if (m_addrKeys.size() == 0)
        return false;

    if (address < m_symbols[m_addrKeys[0].second].address)
        return false;

    // Binary chop
    size_t lower = 0;
    size_t upper_plus_one = m_addrKeys.size();

    // We need to find the first item which is *lower or equal* to N,
    while ((lower + 1) < upper_plus_one)
    {
        size_t mid = (lower + upper_plus_one) / 2;

        const Symbol& midSym = m_symbols[m_addrKeys[mid].second];
        if (midSym.address <= address)
        {
            // mid is lower/equal, search
            lower = mid;
        }
        else {
            // mid is
            upper_plus_one = mid;
        }
    }

    result = m_symbols[m_addrKeys[lower].second];
    return true;
}
