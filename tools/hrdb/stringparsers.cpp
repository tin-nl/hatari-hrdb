#include "stringparsers.h"
#include <vector>
#include <string.h>

#include "symboltable.h"
#include "registers.h"
//-----------------------------------------------------------------------------
bool StringParsers::ParseHexChar(char c, uint8_t& result)
{
    if (c >= '0' && c <= '9')
    {
        result = (uint8_t)(c - '0');
        return true;
    }
    if (c >= 'a' && c <= 'f')
    {
        result = (uint8_t)(10 + c - 'a');
        return true;
    }
    if (c >= 'A' && c <= 'F')
    {
        result = (uint8_t)(10 + c - 'A');
        return true;
    }
    result = 0;
    return false;
}

//-----------------------------------------------------------------------------
bool StringParsers::ParseHexString(const char *pText, uint32_t& result)
{
    result = 0;
    while (*pText != 0)
    {
        uint8_t val;
        if (!ParseHexChar(*pText, val))
        {
            result = 0;
            return false;
        }
        result <<= 4;
        result |= val;
        ++pText;
    }
    return true;
}

struct Token
{
    enum Type
    {
        CONSTANT,
        OPERATOR,
    };
    Type        type;
    uint64_t    val;        // for constants
};

static bool IsWhitespace(char c)
{
    return c == ' ' || c == '\t';
}

static bool IsDecimalDigit(char c)
{
    return c >= '0' && c <= '9';
}


static bool IsAlphaLower(char c)
{
    return c >= 'a' && c <= 'z';
}

static bool IsAlphaUpper(char c)
{
    return c >= 'A' && c <= 'Z';
}

static bool IsAlpha(char c)
{
    return IsAlphaLower(c) || IsAlphaUpper(c);
}

static bool IsSymbolStart(char c)
{
    return IsAlphaLower(c) || IsAlphaUpper(c) || IsDecimalDigit(c) || c == '_';
}

static bool IsSymbolMain(char c)
{
    return IsAlphaLower(c) || IsAlphaUpper(c) || IsDecimalDigit(c) || c == '_';
}

static bool IsRegisterName(const char* name, int& regId)
{
    for (int i = 0; i < Registers::REG_COUNT; ++i)
    {
        if (strcasecmp(name, Registers::s_names[i]) != 0)
            continue;

        regId = i;
        return true;
    }
    regId = Registers::REG_COUNT;
    return false;
}

//-----------------------------------------------------------------------------
bool StringParsers::ParseExpression(const char *pText, uint32_t &result, const SymbolTable& syms, const Registers& regs)
{
    std::vector<Token> tokens;
    for (;*pText != 0;)
    {
        // Decide on next token type
        if (IsWhitespace(*pText))
        {
            ++pText;
            continue;
        }

        if (*pText == '$')
        {
            // Hex constant
            Token t;
            t.type = Token::CONSTANT;
            t.val = 0;
            uint8_t ch;
            ++pText;    // skip $
            while (ParseHexChar(*pText, ch))
            {
                t.val <<= 4;
                t.val |= ch;
                ++pText;
            }
            tokens.push_back(t);
            continue;
        }
        else if (IsSymbolStart(*pText))
        {
            // Possible symbol
            std::string name;
            name += *pText++;
            while (IsSymbolMain(*pText))
                name += *pText++;

            // This might be a register name, use in preference to symbols
            int regId = 0;
            if (IsRegisterName(name.c_str(), regId))
            {
                Token t;
                t.type = Token::CONSTANT;
                t.val = regs.m_value[regId];
                tokens.push_back(t);
                continue;
            }

            // Try to look up symbols by name
            Symbol s;
            if (syms.Find(name, s))
            {
                Token t;
                t.type = Token::CONSTANT;
                t.val = s.address;
                tokens.push_back(t);
                continue;
            }
        }
        return false;
    }

    if (tokens.size() != 0)
    {
        result = tokens[0].val;
        return true;
    }
    return false;
}
