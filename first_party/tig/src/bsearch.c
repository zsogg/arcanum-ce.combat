#include "tig/bsearch.h"

// CE: a plain halving search. The original is the CRT's (half = num / 2,
// less one when num is even, 0x53796f), so with an even count it probes the
// lower middle. A missing key gives the same insertion point; a duplicated
// one may be found at another copy, which its one caller, `mes_find_next`,
// steps past in full.
//
// 0x537940
void* tig_bsearch(void const* key, void const* base, size_t cnt, size_t size, int (*fn)(void const*, void const*), int* exists_ptr)
{
    unsigned char* l = (unsigned char*)base;
    unsigned char* r = l + size * cnt;
    unsigned char* mid;
    int cmp;

    while (l < r) {
        mid = l + size * ((r - l) / size / 2);
        cmp = fn(key, mid);

        if (cmp == 0) {
            *exists_ptr = 1;
            return mid;
        } else if (cmp < 0) {
            r = mid;
        } else {
            l = mid + size;
        }
    }

    *exists_ptr = 0;
    return l;
}
