#ifndef ARCANUM_GAME_CE_PARSE_H_
#define ARCANUM_GAME_CE_PARSE_H_

#include <stdbool.h>

// CE: a whole-number parse that can fail, for CE's own configuration files.
//
// `tig_str_parse_named_value` (0x532320) reads with `atoi`, which cannot
// report failure, so a value of `banana` read as a legal-looking 0. That parser
// is Faithful and shipped data goes through it; CE's own surfaces can be
// strict. Leading and trailing blanks are allowed; anything else around the
// number, an empty string, or a value outside `int` is a failure, and
// `*value` is then left as it was.
//
// Given a home on 2026-09-25, when the reload rules became its third user:
// `ai.c` (rules/ai_fatigue.mes) and `highres_config.c` had carried a copy
// each, with a note that a third meant it got one.
bool ce_parse_int(const char* str, int* value);

#endif /* ARCANUM_GAME_CE_PARSE_H_ */
