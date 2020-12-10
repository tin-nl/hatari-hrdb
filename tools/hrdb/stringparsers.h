#ifndef STRINGPARSERS_H
#define STRINGPARSERS_H

#include <stdint.h>

class StringParsers
{
public:
    // Convert a single hex char to a 0-15 value
    // returns false if char is invalid
    static bool ParseHexChar(char c, uint8_t &result);

    // Convert a (non-prefixed) hex value string to a u32
    // returns false if any char is invalid
    static bool ParseHexString(const char *pText, uint32_t &result);
};

#endif // STRINGPARSERS_H
