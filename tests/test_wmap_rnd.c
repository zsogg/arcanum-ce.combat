// Porting bug #58 (2026-10-20): `wmap_rnd_mod_load` read every line of a
// random-encounter table from the table's first line - the original advances
// the line number after each entry (0x558513-0x558522) - so each table
// produced only its first encounter. This loads a small `Rules/WMap_Rnd.mes`
// through the real loader and reads back what each entry holds.
#include <stdio.h>

#include "game/gamelib.h"
#include "game/settings.h"
#include "obj_harness.h"
#include "test_harness.h"
#include "tig/file.h"
#include "ui/wmap_rnd.h"

#define WMAP_RND_FILE "rules/WMap_Rnd.mes"

static bool wmap_begin(void)
{
    FILE* stream;
    int table;

    settings_init(&settings, "unused");
    if (!obj_harness_init()) {
        return false;
    }
    tig_file_mkdir("rules");
    stream = fopen(WMAP_RND_FILE, "wb");
    if (stream == NULL) {
        return false;
    }
    // One circle in each chart, no special tables, and three distinct lines
    // in table 0: different weights, critters and level ranges.
    fputs("{100}{0, 0}\n"
          "{10000}{1000, 1000, 500, 5%}\n"
          "{20000}{1000, 1000, 500, Easy}\n"
          "{30000}{90000, 90000, 1, 0}\n"
          "{49999}{0}\n"
          "{50000}{80%, First: 28427, 1-4, MaxLevel: 10}\n"
          "{50001}{30%, First: 28428, 1, MinLevel: 3, MaxLevel: 15}\n"
          "{50002}{20%, First: 28355, 2-4, MinLevel: 26}\n",
        stream);
    // The loader reads all 54 terrain tables and refuses an empty one.
    for (table = 1; table < 54; table++) {
        fprintf(stream, "{%d}{50%%, First: 28427, 1}\n", 50000 + 100 * table);
    }
    fclose(stream);
    return wmap_rnd_mod_load();
}

static void wmap_end(void)
{
    wmap_rnd_mod_unload();
    remove(WMAP_RND_FILE);
    obj_harness_exit();
    tig_file_repository_remove(".");
    settings_exit(&settings);
}

TEST(each_line_of_a_table_is_its_own_entry)
{
    int message_num = 0;
    int frequency = 0;
    int min_level = 0;
    int max_level = 0;

    CHECK(wmap_begin());
    CHECK(wmap_rnd_entry_info(0, 0, &message_num, &frequency, &min_level, &max_level));
    CHECK_INT(message_num, 50000);
    CHECK_INT(frequency, 80);
    CHECK_INT(max_level, 10);
    CHECK(wmap_rnd_entry_info(0, 1, &message_num, &frequency, &min_level, &max_level));
    CHECK_INT(message_num, 50001);
    CHECK_INT(frequency, 30);
    CHECK_INT(min_level, 3);
    CHECK_INT(max_level, 15);
    CHECK(wmap_rnd_entry_info(0, 2, &message_num, &frequency, &min_level, &max_level));
    CHECK_INT(message_num, 50002);
    CHECK_INT(frequency, 20);
    CHECK_INT(min_level, 26);
    CHECK(!wmap_rnd_entry_info(0, 3, &message_num, &frequency, &min_level, &max_level));
    wmap_end();
}

void test_wmap_rnd_suite(void)
{
    RUN(each_line_of_a_table_is_its_own_entry);
}
