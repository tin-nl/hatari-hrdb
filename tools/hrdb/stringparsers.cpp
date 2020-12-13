#include "stringparsers.h"
#include <vector>

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

//-----------------------------------------------------------------------------
bool StringParsers::ParseExpression(const char *pText, uint32_t &result)
{
    std::vector<Token> tokens;
    for (;*pText != 0; ++pText)
    {
        // Decide on next token type
        if (IsWhitespace(*pText))
            continue;

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
        }
    }

    if (tokens.size() != 0)
    {
        result = tokens[0].val;
        return true;
    }
    return false;
}
