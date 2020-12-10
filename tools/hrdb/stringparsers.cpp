#include "stringparsers.h"

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
