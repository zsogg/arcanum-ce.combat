#include "game/provenance.h"

#include <stdio.h>
#include <string.h>

#include "game/area.h"
#include "game/feature.h"
#include "game/gamelib.h"
#include "tig/tig.h"

// Where the two files land, inside `Save\Current` - so they are archived into
// the `.tfai`/`.tfaf` pair by `gamelib_save` along with everything else, come
// back on load, and travel with a copied save. That is the whole reason this
// is a pair of files in the save folder rather than bytes in `data.sav`:
// **no stock subsystem's framing changes.** `data.sav`, `data2.sav` and the
// `.gsi` are byte-for-byte what they were, and a reader that has never heard
// of provenance never encounters it.
#define PROVENANCE_PATH "Save\\Current\\ce-provenance.json"
#define MODS_RECORD_PATH "Save\\Current\\ce-mods.json"

// Written by `arcanum-work/mods/modkit` at install and uninstall time, at the
// game root. The engine cannot know the mod set: mods are loose files in
// layers it merges, with nothing marking who put them there. What it can do is
// carry the toolkit's own record into the save unchanged, which is what this
// is - copied verbatim, never parsed. A malformed record therefore cannot
// produce a malformed provenance file, and a record from a toolkit newer than
// this build travels intact.
//
// The leading `.\` matters: `tig_file_fopen` only falls through to the real
// filesystem for a path starting `.`, `\`, `/` or with `:` second. Without it
// this would be looked up in the archive repositories, where it will never be.
#define MODS_RECORD_SOURCE ".\\ce-mods.json"

#define PROVENANCE_SCHEMA 1

// **Nothing written here may vary between two runs of one build on one
// configuration.** `arcanum-work/harness/determinism-check.sh` runs CE twice
// against one save and compares the two save trees byte for byte, and these
// files are in that comparison. So there is no timestamp, no path, no
// process id and no address anywhere below - a provenance record says *what*,
// never *when*, which is also what makes it worth comparing between saves.

static bool write_escaped(TigFile* stream, const char* text)
{
    // Enough JSON string escaping for what actually reaches here: a module
    // name from the config, and feature keys from `feature_list.h`. Control
    // characters go out as \uXXXX rather than raw, because a raw one would
    // make the file unreadable by a strict parser for no benefit.
    char buffer[8];

    if (tig_file_fputc('"', stream) == -1) {
        return false;
    }

    for (; *text != '\0'; text++) {
        unsigned char ch = (unsigned char)*text;
        const char* escape = NULL;

        switch (ch) {
        case '"':
            escape = "\\\"";
            break;
        case '\\':
            escape = "\\\\";
            break;
        case '\n':
            escape = "\\n";
            break;
        case '\r':
            escape = "\\r";
            break;
        case '\t':
            escape = "\\t";
            break;
        default:
            break;
        }

        if (escape != NULL) {
            if (tig_file_fputs(escape, stream) == -1) {
                return false;
            }
        } else if (ch < 0x20) {
            snprintf(buffer, sizeof(buffer), "\\u%04x", ch);
            if (tig_file_fputs(buffer, stream) == -1) {
                return false;
            }
        } else {
            if (tig_file_fputc((char)ch, stream) == -1) {
                return false;
            }
        }
    }

    return tig_file_fputc('"', stream) != -1;
}

static bool write_guid(TigFile* stream)
{
    TigGuid guid;
    char text[33];
    int index;

    if (!gamelib_mod_guid_get(&guid)) {
        return tig_file_fputs("null", stream) != -1;
    }

    for (index = 0; index < 16; index++) {
        snprintf(&(text[index * 2]), 3, "%02x", guid.data[index]);
    }
    text[32] = '\0';

    return write_escaped(stream, text);
}

// Copies the toolkit's record into the save if there is one. Absence is the
// normal case - a stock install has no mods and no record - and is reported as
// `false` so the caller can say `"mods": null` rather than pretending.
static bool copy_mods_record(void)
{
    TigFile* src;
    TigFile* dst;
    char buffer[4096];
    size_t got;
    bool ok = true;

    src = tig_file_fopen(MODS_RECORD_SOURCE, "rb");
    if (src == NULL) {
        // **Remove any stale copy first.** `gamelib_load` unarchives a save
        // back into this folder, so a record from the save that was loaded is
        // sitting here; without this, saving after uninstalling every mod
        // would archive the old mod list again and the provenance would name
        // mods that are gone. Found by reading the load path, not by a run.
        tig_file_remove(MODS_RECORD_PATH);
        return false;
    }

    dst = tig_file_fopen(MODS_RECORD_PATH, "wb");
    if (dst == NULL) {
        tig_file_fclose(src);
        return false;
    }

    while ((got = tig_file_fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (tig_file_fwrite(buffer, 1, got, dst) != got) {
            ok = false;
            break;
        }
    }

    tig_file_fclose(src);
    tig_file_fclose(dst);

    if (!ok) {
        tig_file_remove(MODS_RECORD_PATH);
    }

    return ok;
}

bool provenance_save(void)
{
    TigFile* stream;
    bool has_mods;
    int index;
    char buffer[64];
    bool ok = false;

    has_mods = copy_mods_record();

    stream = tig_file_fopen(PROVENANCE_PATH, "wb");
    if (stream == NULL) {
        return false;
    }

    do {
        snprintf(buffer, sizeof(buffer), "{\n  \"schema\": %d,\n", PROVENANCE_SCHEMA);
        if (tig_file_fputs(buffer, stream) == -1) break;

        if (tig_file_fputs("  \"engine\": {\n    \"port\": \"arcanum-ce\",\n", stream) == -1) break;

        // The save-stream version the modules were written at, and the CE
        // departures a reader has to know about to make sense of them. This
        // is the useful half of "engine identity": not which commit built the
        // binary, but what the bytes in this save mean.
        snprintf(buffer, sizeof(buffer), "    \"save_version\": %d,\n", 25);
        if (tig_file_fputs(buffer, stream) == -1) break;
        if (tig_file_fputs("    \"capabilities\": [\"area-count-prefix\"]\n  },\n", stream) == -1) break;

        if (tig_file_fputs("  \"module\": { \"name\": ", stream) == -1) break;
        if (!write_escaped(stream, gamelib_current_module_name_get())) break;
        if (tig_file_fputs(", \"guid\": ", stream) == -1) break;
        if (!write_guid(stream)) break;
        if (tig_file_fputs(" },\n", stream) == -1) break;

        // The area count is here because it is the one number that has ever
        // made a save unreadable (MODDING-NOTES.md quirk 17), and because a
        // save whose count differs from the current list is exactly the case
        // worth seeing when diagnosing one.
        snprintf(buffer, sizeof(buffer), "  \"areas\": %d,\n", area_count());
        if (tig_file_fputs(buffer, stream) == -1) break;

        if (tig_file_fputs("  \"features\": {", stream) == -1) break;
        for (index = 0; index < FEATURE_COUNT; index++) {
            if (index != 0 && tig_file_fputs(",", stream) == -1) break;
            if (tig_file_fputs(" ", stream) == -1) break;
            if (!write_escaped(stream, feature_key((FeatureId)index))) break;
            if (tig_file_fputs(feature_enabled((FeatureId)index) ? ": true" : ": false",
                    stream)
                == -1) {
                break;
            }
        }
        if (index != FEATURE_COUNT) break;
        if (tig_file_fputs(" },\n", stream) == -1) break;

        // A pointer to the sibling file rather than its contents. Copying it
        // verbatim is what keeps a malformed or newer toolkit record from
        // making this file malformed too.
        if (tig_file_fputs(has_mods ? "  \"mods\": \"ce-mods.json\"\n}\n"
                                    : "  \"mods\": null\n}\n",
                stream)
            == -1) {
            break;
        }

        ok = true;
    } while (0);

    tig_file_fclose(stream);

    if (!ok) {
        tig_file_remove(PROVENANCE_PATH);
    }

    return ok;
}
