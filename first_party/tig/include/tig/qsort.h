#ifndef TIG_QSORT_H_
#define TIG_QSORT_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void tig_qsort(void* base, size_t cnt, size_t size, int (*fn)(const void*, const void*));

#ifdef __cplusplus
}
#endif

#endif /* TIG_QSORT_H_ */
