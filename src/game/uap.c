#include "game/uap.h"

#include "game/gamelib.h"
#include "game/settings.h"

static void uap_fixes_changed(void);

static bool uap_fixes;

void uap_init(void)
{
    settings_register(&settings, UAP_FIXES_KEY, "0", uap_fixes_changed);
    uap_fixes_changed();
}

bool uap_fixes_enabled(void)
{
    return uap_fixes;
}

void uap_fixes_changed(void)
{
    uap_fixes = settings_get_value(&settings, UAP_FIXES_KEY) != 0;
}
