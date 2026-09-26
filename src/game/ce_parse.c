#include "game/ce_parse.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>

bool ce_parse_int(const char* str, int* value)
{
    char* end;
    long parsed;

    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {
        return false;
    }

    parsed = strtol(str, &end, 10);
    if (end == str) {
        return false;
    }

    // Trailing anything that is not blank means the author wrote something
    // this does not understand, and guessing at it is what card X7 undid.
    while (*end != '\0') {
        if (!isspace((unsigned char)*end)) {
            return false;
        }
        end++;
    }

    if (parsed < INT_MIN || parsed > INT_MAX) {
        return false;
    }

    *value = (int)parsed;
    return true;
}
