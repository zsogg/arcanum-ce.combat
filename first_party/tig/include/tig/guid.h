#ifndef TIG_GUID_H_
#define TIG_GUID_H_

#include "tig/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TigGuid {
    uint8_t data[16];
} TigGuid;

// Serializeable.
static_assert(sizeof(TigGuid) == 16, "wrong size");

// Reseeds the GUID generator, for reproducible runs.
//
// `tig_guid_init` seeds from `time(NULL)`, so every run produces different
// GUIDs. A GUID reaches disk - `tig_database_create` writes one into a .dat
// header, and the module layer keeps them - so a time-seeded generator is one
// of the things that stops two runs of the same save being comparable.
//
// Off the deterministic path nothing calls this and behaviour is unchanged.
void tig_guid_seed_set(unsigned int seed);

int tig_guid_init(TigInitInfo* init_info);
void tig_guid_exit(void);
void tig_guid_create(TigGuid* guid);
bool tig_guid_is_equal(const TigGuid* a, const TigGuid* b);

#ifdef __cplusplus
}
#endif

#endif /* TIG_GUID_H_ */
