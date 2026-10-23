// The module name is the one outside input the module-path builders take
// (a save's .gsi, up to 255 bytes; `-mod:` on the command line, up to 254),
// and until 2026-10-22 it was copied into 260-byte buffers by `strcpy` and
// `strcat` chains (review finding 5). Both cases below overflowed under ASan
// against the previous code.
#include <string.h>

#include "game/gamelib.h"
#include "test_harness.h"
#include "tig/file.h"

static char long_name[301];

static void fill_long_name(size_t length)
{
    memset(long_name, 'm', length);
    long_name[length] = '\0';
}

TEST(a_default_module_name_longer_than_a_path_is_cut_not_copied_past_its_buffer)
{
    const char* saved = gamelib_default_module_name_get();
    char restore[TIG_MAX_PATH];

    strcpy(restore, saved);
    fill_long_name(300);
    gamelib_default_module_name_set(long_name);
    CHECK((int)strlen(gamelib_default_module_name_get()) < TIG_MAX_PATH);
    gamelib_default_module_name_set(restore);
    CHECK_STR(gamelib_default_module_name_get(), restore);
}

// Through the public entry: with no module loaded, `gamelib_mod_load` only
// unloads nothing and then builds the module path, which is where the
// refusal sits - before the directory the loader otherwise creates.
TEST(a_module_name_that_cannot_fit_its_path_is_refused_before_any_directory_is_made)
{
    fill_long_name(250);
    CHECK(!gamelib_mod_load(long_name));
    CHECK(!tig_file_is_directory(".\\Modules\\"
                                 "mmmm"));
}

void test_module_paths_suite(void)
{
    RUN(a_default_module_name_longer_than_a_path_is_cut_not_copied_past_its_buffer);
    RUN(a_module_name_that_cannot_fit_its_path_is_refused_before_any_directory_is_made);
}
