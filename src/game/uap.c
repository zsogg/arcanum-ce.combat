#include "game/uap.h"

#include <tig/tig.h>

#include "game/gamelib.h"
#include "game/highres_config.h"
#include "game/obj.h"
#include "game/settings.h"

static void uap_fixes_changed(void);

static bool uap_fixes;
static UapDragItemQuery uap_drag_item_query;

void uap_init(void)
{
    settings_register(&settings, UAP_FIXES_KEY, "0", uap_fixes_changed);
    uap_fixes_changed();
}

bool uap_fixes_enabled(void)
{
    return uap_fixes;
}

void uap_drag_item_query_set(UapDragItemQuery query)
{
    uap_drag_item_query = query;
}

bool uap_drag_item_active(void)
{
    // NOTE: The patch compares only the low dword of the handle
    // (`cmp DWORD PTR ds:0x6810E0, 0`). A handle whose low half is zero while
    // its high half is not would read as "not dragging" there and as
    // "dragging" here. Not reproduced - the whole-handle test is what the
    // patch means, and CE has no notion of half a handle.
    return uap_drag_item_query != NULL
        && uap_drag_item_query() != OBJ_HANDLE_NULL;
}

void uap_fixes_changed(void)
{
    uap_fixes = settings_get_value(&settings, UAP_FIXES_KEY) != 0;

    // TIG sits below the game and cannot ask, so push the switch down to it.
    tig_art_uap_fixes_set(uap_fixes);

    // That call also decides sprite mirroring, so the high resolution patch's
    // `SpriteMirroring` only gets a say once the patch's art is out of the
    // picture. With uap fixes on, mirroring must stay off whatever config.ini
    // says, because those archives already contain the mirrored frames.
    if (!uap_fixes) {
        tig_art_mirroring_set(highres_config_get()->sprite_mirroring);
    }
}
