#ifndef TIG_DEBUG_H_
#define TIG_DEBUG_H_

#include <SDL3/SDL_stdinc.h>

#include "tig/types.h"

#ifdef __cplusplus
extern "C" {
#endif

int tig_debug_init(TigInitInfo* init_info);
void tig_debug_exit(void);
/**
 * NOTE: Annotated so the compiler checks every call site. Without this GCC
 * silently accepted format/argument mismatches, including conversions with no
 * argument at all - see the fixes in c384899 and 40e1022.
 */
void tig_debug_printf(SDL_PRINTF_FORMAT_STRING const char* format, ...) SDL_PRINTF_VARARG_FUNC(1);
void tig_debug_println(const char* string);

#ifdef __cplusplus
}
#endif

#endif /* TIG_DEBUG_H_ */
