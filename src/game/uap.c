#include "game/uap.h"

#include <tig/tig.h>

#include "game/feature.h"
#include "game/highres_config.h"
#include "game/obj.h"

static void uap_fixes_changed(void);

static UapDragItemQuery uap_drag_item_query;

void uap_init(void)
{
    // The key, the default and the current value belong to the registry now
    // (`game/feature_list.h`); what stays here is the side effect, because it
    // reaches into tig and into the high resolution patch and neither belongs
    // in a table. `feature_init` has already read the config by this point.
    //
    // Attaching a callback does not fire it, exactly as `settings_register`
    // never did, so the explicit call below is the same one this function has
    // always made -- and it is what pushes the switch down to tig at the same
    // point in init as before.
    feature_on_change(FEATURE_UAP_FIXES, uap_fixes_changed);
    uap_fixes_changed();
}

bool uap_fixes_enabled(void)
{
    return feature_enabled(FEATURE_UAP_FIXES);
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
    bool uap_fixes = feature_enabled(FEATURE_UAP_FIXES);

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
