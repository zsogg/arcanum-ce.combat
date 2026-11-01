#include "tig/strcase.h"

// FIX (porting bug #375): the game's case-insensitive comparisons are the
// compiler's `_stricmp` and `_strnicmp`, linked into `Arcanum.exe`. They fold
// A-Z to a-z and compare every other byte as it is, so in the game's
// Windows-1252 text "é" and "è", or "Ä" and "ä", are different strings. CE
// called SDL's, which decode both strings as UTF-8 and turn every byte that
// starts no valid sequence into U+FFFD, so all of those compared equal. These
// are the original's: the same folding, and -1, 0 or 1 as it returns, checked
// against it by emudiff. ASCII compares as before.
static int tig_strcase_fold(const char* p)
{
    int c = (unsigned char)*p;

    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

// 0x58ADB0
int tig_strcasecmp(const char* a, const char* b)
{
    int ca;
    int cb;

    do {
        ca = tig_strcase_fold(a++);
        cb = tig_strcase_fold(b++);
    } while (ca != 0 && ca == cb);

    return (ca > cb) - (ca < cb);
}

// 0x595AA0
int tig_strncasecmp(const char* a, const char* b, size_t n)
{
    int ca;
    int cb;

    if (n == 0) {
        return 0;
    }

    do {
        ca = tig_strcase_fold(a++);
        cb = tig_strcase_fold(b++);
    } while (--n != 0 && ca != 0 && ca == cb);

    return (ca > cb) - (ca < cb);
}
