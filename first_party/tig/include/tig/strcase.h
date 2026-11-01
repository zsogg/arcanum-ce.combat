#ifndef TIG_STRCASE_H_
#define TIG_STRCASE_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int tig_strcasecmp(const char* a, const char* b);
int tig_strncasecmp(const char* a, const char* b, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* TIG_STRCASE_H_ */
