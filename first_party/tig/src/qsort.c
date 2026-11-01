#include "tig/qsort.h"

// The original's ranges deferred while it sorts another; recursing into the
// smaller side first keeps the depth under log2 of the count.
#define TIG_QSORT_STACK 30

// Ranges this short are finished by selection rather than partitioned.
#define TIG_QSORT_CUTOFF 8

static void tig_qsort_selection(unsigned char* lo, unsigned char* hi, size_t size, int (*fn)(const void*, const void*));
static void tig_qsort_swap(unsigned char* a, unsigned char* b, size_t size);

// FIX (porting bug #374): Arcanum sorts with the qsort its compiler linked
// into the executable, and the game reads the order it leaves equal elements
// in: which spell the AI tries first among equal priorities (`mt_ai`), and
// which world-map region of equal radius a location falls in (`wmap_rnd`).
// CE called the platform's qsort, which orders ties its own way: glibc keeps
// them in input order, the original does not, and Apple's matches neither.
// This is the original's algorithm, so every platform orders ties as
// `Arcanum.exe` does: the middle element is swapped to the front as the
// pivot, two cursors close in from both ends, the smaller side is sorted next
// and the larger deferred, and ranges of `TIG_QSORT_CUTOFF` or fewer are
// finished by selection.
//
// 0x58A548
void tig_qsort(void* base, size_t cnt, size_t size, int (*fn)(const void*, const void*))
{
    unsigned char* lo_stack[TIG_QSORT_STACK];
    unsigned char* hi_stack[TIG_QSORT_STACK];
    int depth;
    unsigned char* lo;
    unsigned char* hi;
    unsigned char* left;
    unsigned char* right;
    size_t n;

    if (cnt < 2 || size == 0) {
        return;
    }

    depth = 0;
    lo = (unsigned char*)base;
    hi = lo + size * (cnt - 1);

    for (;;) {
        n = (size_t)(hi - lo) / size + 1;
        if (n <= TIG_QSORT_CUTOFF) {
            tig_qsort_selection(lo, hi, size, fn);
        } else {
            tig_qsort_swap(lo + (n / 2) * size, lo, size);

            left = lo;
            right = hi + size;
            for (;;) {
                do {
                    left += size;
                } while (left <= hi && fn(left, lo) <= 0);

                do {
                    right -= size;
                } while (right > lo && fn(right, lo) >= 0);

                if (right < left) {
                    break;
                }

                tig_qsort_swap(left, right, size);
            }

            tig_qsort_swap(lo, right, size);

            // `right` holds the pivot; `lo..right - size` and `left..hi`
            // remain. The larger waits on the stack.
            if ((right - lo) - 1 >= hi - left) {
                if (lo + size < right) {
                    lo_stack[depth] = lo;
                    hi_stack[depth] = right - size;
                    depth++;
                }

                if (left < hi) {
                    lo = left;
                    continue;
                }
            } else {
                if (left < hi) {
                    lo_stack[depth] = left;
                    hi_stack[depth] = hi;
                    depth++;
                }

                if (lo + size < right) {
                    hi = right - size;
                    continue;
                }
            }
        }

        depth--;
        if (depth < 0) {
            return;
        }

        lo = lo_stack[depth];
        hi = hi_stack[depth];
    }
}

// Moves the largest of `lo..hi` to `hi` and repeats one shorter; of equal
// largest elements the first found stays chosen.
//
// 0x58A69C
static void tig_qsort_selection(unsigned char* lo, unsigned char* hi, size_t size, int (*fn)(const void*, const void*))
{
    unsigned char* max;
    unsigned char* p;

    while (hi > lo) {
        max = lo;
        for (p = lo + size; p <= hi; p += size) {
            if (fn(p, max) > 0) {
                max = p;
            }
        }

        tig_qsort_swap(max, hi, size);
        hi -= size;
    }
}

// 0x58A6EA
static void tig_qsort_swap(unsigned char* a, unsigned char* b, size_t size)
{
    unsigned char tmp;

    if (a == b) {
        return;
    }

    while (size-- != 0) {
        tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}
