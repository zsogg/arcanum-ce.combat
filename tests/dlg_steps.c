// Runs main-line dialogue steps through CE's own dialogue code (QA1 phase 2
// item 2).
//
// For each case: load a shipped .dlg with CE's loader, build a PC and the NPC
// with the object harness, set a "before" state (story state, global flags
// and variables, PC quest states, the NPC's dialogue local flags, PC stats,
// items, gold, followers, rumours, reaction), then
//
//   (a) collect the PC options at a node with `dialog_collect_pc_options`,
//       and, with the same `dialog_option_qualifies` test, every option that
//       is eligible before the five-slot cap;
//   (b) perform the chosen option's actions with `dialog_perform_actions`,
//       then the actions of the NPC line it leads to (what
//       `dialog_build_npc_reply` does when that line is shown), and print the
//       watched state.
//
//     arcanum-dlg-steps <repo-list> <case-file> [<layer>]
//
// `<repo-list>` is one repository per line, lowest priority first, as for
// `arcanum-dlg-dump`. `<layer>`, if given, is mounted over them: a directory
// laid out as the game's `data/`, such as a mod's `modkit build` output, so
// the cases run against the mod's files. The case file, one command per line:
//
//     case <id> <node> <option or -1> <npc line or -1> <dlg path, to the end>
//     ss N | gf N V | gv N V | qu N S | lf N V
//     stat S V | skill B POINTS | gold N | item N | npcitem N | follower N
//     rumor N | reaction N
//     watch ss | watch gf N | watch gv N | watch qu N
//     run
//     reply                                    build the NPC reply at <node>
//
// Output, per case:
//
//     C <id>
//     O <offered count> <entry nums>           dialog_collect_pc_options
//     E <eligible count> <entry nums>          no cap
//     P <option> <offered 0|1> <eligible 0|1>
//     X <call>                                 a call out of the dialogue (below)
//     B <key>=<value> ...                      watched state, before
//     A <key>=<value> ...                      watched state, after
//     R <returned 0|1> <status> <num_options>  `reply`: dialog_build_npc_reply,
//                                              with num_options preset to 7
//
// Quest, story-state, flag and variable state is the engine's own
// (`script_init`, `quest_init` then `quest_reset`, as a new game does: without
// the reset every global quest state is 0, not "accepted", and
// `quest_state_set_internal` botches each quest it is asked to set). The calls
// that need a world - XP, item and gold transfers and the item's removal,
// reputation, combat, newspapers, and whether the NPC can speak - are wrapped
// (`-Wl,--wrap`, tests/CMakeLists.txt): each prints an X line and does nothing
// (`ai_can_speak` answers yes), so none can change the state watched here.
// `mm` and `ru` run for real and change nothing visible: the harness has no
// local PC for the area flags, and its clock is 0, a rumour's "unknown".
//
// Not registered as a test: it needs the game's archives. `dialog.c` is
// included, as in dlg_dump.c, for its static functions. Mount only `.dat`
// files and a `<layer>` without a `TIGCache` (one with is refused): mounting a
// directory deletes its `TIGCache`. Run it from a scratch
// directory: the object harness writes `rules/effect.mes` under the cwd.

#include "../src/game/dialog.c"

#include "obj_harness.h"

#define MAX_WATCH 64
#define MAX_OPTS 256

static int64_t pc_obj;
static int64_t npc_obj;
static char watch_kind[MAX_WATCH];
static int watch_num[MAX_WATCH];
static int watch_count;

// Wrapped callees: a line, and nothing else.
void __wrap_critter_give_xp(int64_t obj, int xp_gain)
{
    (void)obj;
    printf("X\tcritter_give_xp %d\n", xp_gain);
}

bool __wrap_item_gold_transfer(int64_t from_obj, int64_t to_obj, int qty, int64_t gold_obj)
{
    (void)gold_obj;
    printf("X\titem_gold_transfer %s->%s %d\n",
        from_obj == pc_obj ? "pc" : from_obj == OBJ_HANDLE_NULL ? "none"
                                                                : "npc",
        to_obj == pc_obj ? "pc" : "npc", qty);
    return true;
}

bool __wrap_item_transfer(int64_t item_obj, int64_t critter_obj)
{
    printf("X\titem_transfer %d to %s\n", obj_field_int32_get(item_obj, OBJ_F_NAME),
        critter_obj == pc_obj ? "pc" : "npc");
    return true;
}

void __wrap_object_destroy(int64_t obj)
{
    printf("X\tobject_destroy %d\n", obj_field_int32_get(obj, OBJ_F_NAME));
}

void __wrap_item_remove(int64_t item_obj)
{
    printf("X\titem_remove %d\n", obj_field_int32_get(item_obj, OBJ_F_NAME));
}

void __wrap_item_drop_at_loc(int64_t item_obj, int64_t loc)
{
    (void)loc;
    printf("X\titem_drop_at_loc %d\n", obj_field_int32_get(item_obj, OBJ_F_NAME));
}

void __wrap_reputation_add(int64_t obj, int reputation)
{
    (void)obj;
    printf("X\treputation_add %d\n", reputation);
}

void __wrap_reputation_remove(int64_t obj, int reputation)
{
    (void)obj;
    printf("X\treputation_remove %d\n", reputation);
}

void __wrap_ai_attack(int64_t target_obj, int64_t source_obj, int loudness, unsigned int flags)
{
    (void)target_obj;
    (void)source_obj;
    (void)loudness;
    (void)flags;
    printf("X\tai_attack\n");
}

void __wrap_newspaper_enqueue(int num, bool priority)
{
    printf("X\tnewspaper_enqueue %d %d\n", num, priority);
}

// `fl` floats a line only if the NPC can speak, which reads spell state the
// harness has no magictech system for. The harness NPC is awake and well.
int __wrap_ai_can_speak(int64_t npc_obj, int64_t pc_obj, bool a3)
{
    (void)npc_obj;
    (void)pc_obj;
    (void)a3;
    return AI_SPEAK_OK;
}

static int read_lines(const char* path, char*** out)
{
    FILE* stream;
    char** lines = NULL;
    int count = 0;
    int capacity = 0;
    char buffer[TIG_MAX_PATH];

    stream = fopen(path, "rb");
    if (stream == NULL) {
        fprintf(stderr, "cannot read %s\n", path);
        return -1;
    }

    while (fgets(buffer, sizeof(buffer), stream) != NULL) {
        size_t length = strlen(buffer);

        while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r')) {
            buffer[--length] = '\0';
        }

        if (length == 0) {
            continue;
        }

        if (count == capacity) {
            capacity = capacity != 0 ? capacity * 2 : 64;
            lines = (char**)realloc(lines, sizeof(*lines) * capacity);
        }

        lines[count] = strdup(buffer);
        count++;
    }

    fclose(stream);
    *out = lines;
    return count;
}

static void give_item(int64_t owner, int type, int name, int qty)
{
    int64_t item = obj_harness_create_proto(type);

    obj_field_int32_set(item, OBJ_F_NAME, name);
    if (type == OBJ_TYPE_GOLD) {
        obj_field_int32_set(item, OBJ_F_GOLD_QUANTITY, qty);
    }
    item_insert(item, owner, -1);
}

static void print_watch(const char* tag)
{
    int index;

    printf("%s", tag);
    for (index = 0; index < watch_count; index++) {
        switch (watch_kind[index]) {
        case 's':
            printf("\tss=%d", script_story_state_get());
            break;
        case 'f':
            printf("\tgf%d=%d", watch_num[index], script_global_flag_get(watch_num[index]));
            break;
        case 'v':
            printf("\tgv%d=%d", watch_num[index], script_global_var_get(watch_num[index]));
            break;
        case 'q':
            printf("\tqu%d=%d", watch_num[index], quest_state_get(pc_obj, watch_num[index]));
            break;
        }
    }
    printf("\n");
}

// The options a node offers with no cap: `dialog_collect_pc_options`'s two
// passes with `cnt < 5` taken out.
static int collect_uncapped(DialogState* state, int* out)
{
    DialogFile* dialog = &(dialog_files[state->dlg]);
    DialogFileEntry key;
    DialogFileEntry* entry;
    int gender = stat_level_get(state->pc_obj, STAT_GENDER);
    int intelligence = stat_level_get(state->pc_obj, STAT_INTELLIGENCE);
    int idx;
    int cnt = 0;

    if (intelligence > LOW_INTELLIGENCE && critter_is_dumb(state->pc_obj)) {
        intelligence = 1;
    }

    key.num = state->status_num;
    entry = bsearch(&key, dialog->entries, dialog->entries_length, sizeof(key), dialog_entry_compare);
    if (entry == NULL) {
        return 0;
    }

    for (idx = (int)(entry - dialog->entries) + 1; idx < dialog->entries_length && cnt < MAX_OPTS; idx++) {
        entry = &(dialog->entries[idx]);
        if (entry->iq == 0) {
            break;
        }
        if (dialog_option_qualifies(state, entry, gender, intelligence)) {
            out[cnt++] = entry->num;
        }
    }

    for (idx = 0; idx < dialog->entries_length && cnt < MAX_OPTS; idx++) {
        entry = &(dialog->entries[idx]);
        if (entry->parent == state->status_num
            && dialog_option_qualifies(state, entry, gender, intelligence)) {
            out[cnt++] = entry->num;
        }
    }

    return cnt;
}

static bool in_list(const int* list, int count, int num)
{
    int index;

    for (index = 0; index < count; index++) {
        if (list[index] == num) {
            return true;
        }
    }
    return false;
}

static void print_list(char tag, const int* list, int count)
{
    int index;

    printf("%c\t%d", tag, count);
    for (index = 0; index < count; index++) {
        printf("\t%d", list[index]);
    }
    printf("\n");
}

// Runs one entry's actions, on the stored entry as `dialog_search` gives it.
static void perform(DialogState* state, int num)
{
    DialogFileEntry entry;

    entry.num = num;
    if (!dialog_search(state->dlg, &entry)) {
        printf("X\tno entry %d\n", num);
        return;
    }
    dialog_perform_actions(state, entry.actions, 0);
}

static void run_case(const char* path, int node, int option, int npc_line)
{
    DialogState state;
    int dlg;
    int offered[8];
    int eligible[MAX_OPTS];
    int offered_count;
    int eligible_count;

    if (!dialog_load(path, &dlg)) {
        printf("O\tLOAD FAILED %s\n", path);
        return;
    }

    memset(&state, 0, sizeof(state));
    state.dlg = dlg;
    state.pc_obj = pc_obj;
    state.npc_obj = npc_obj;
    state.status_num = node;

    offered_count = dialog_collect_pc_options(&state, offered);
    eligible_count = collect_uncapped(&state, eligible);
    print_list('O', offered, offered_count);
    print_list('E', eligible, eligible_count);
    if (option >= 0) {
        printf("P\t%d\t%d\t%d\n", option,
            in_list(offered, offered_count, option),
            in_list(eligible, eligible_count, option));
    }

    print_watch("B");
    if (option >= 0) {
        perform(&state, option);
    }
    if (npc_line >= 0) {
        perform(&state, npc_line);
    }
    print_watch("A");

    dialog_unload(dlg);
}

// The reply a node shows, as `dialog_build_exchange` builds it before its
// option pass: whether the exchange goes on, and the status it leaves.
static void run_reply(const char* path, int node)
{
    DialogState state;
    int dlg;
    bool returned;

    if (!dialog_load(path, &dlg)) {
        printf("R\tLOAD FAILED %s\n", path);
        return;
    }

    memset(&state, 0, sizeof(state));
    state.dlg = dlg;
    state.pc_obj = pc_obj;
    state.npc_obj = npc_obj;
    state.status_num = node;
    state.num_options = 7;
    returned = dialog_build_npc_reply(&state, false);
    printf("R\t%d\t%d\t%d\n", returned ? 1 : 0, (int)state.status, state.num_options);

    dialog_unload(dlg);
}

static void reset_world(int script_num)
{
    Script scr;

    if (pc_obj != OBJ_HANDLE_NULL) {
        obj_harness_destroy(pc_obj);
        obj_harness_destroy(npc_obj);
    }

    script_reset();
    quest_reset();
    rumor_reset();

    pc_obj = obj_harness_create_proto(OBJ_TYPE_PC);
    npc_obj = obj_harness_create_proto(OBJ_TYPE_NPC);

    // A dialogue's local flags live in the NPC's dialogue script slot, and
    // read as 0 while that slot holds no script.
    memset(&scr, 0, sizeof(scr));
    scr.num = script_num;
    obj_arrayfield_script_set(npc_obj, OBJ_F_SCRIPTS_IDX, SAP_DIALOG, &scr);

    stat_base_set(pc_obj, STAT_INTELLIGENCE, 10);
    watch_count = 0;
}

// The fixture must hold what the cases set; if it did not, every result
// below would measure nothing.
static bool self_check(void)
{
    PcQuestState pqs;

    reset_world(1);
    quest_state_set(pc_obj, 1010, 2, OBJ_HANDLE_NULL);
    script_global_flag_set(1046, 2);
    script_local_flag_set(npc_obj, SAP_DIALOG, 3, 1);
    obj_arrayfield_pc_quest_get(pc_obj, OBJ_F_PC_QUEST_IDX, 1010, &pqs);
    return quest_state_get(pc_obj, 1010) == 2
        && pqs.state == 2
        && script_global_flag_get(1046) == 0
        && script_global_flag_get(1047) == 1
        && script_local_flag_get(npc_obj, SAP_DIALOG, 3);
}

int main(int argc, char** argv)
{
    GameInitInfo init_info;
    char** repos;
    char** lines;
    int repos_count;
    int lines_count;
    int index;
    char path[TIG_MAX_PATH];
    int node = -1;
    int option = -1;
    int npc_line = -1;
    char id[128];

    if (argc != 3 && argc != 4) {
        fprintf(stderr, "usage: %s <repo-list> <case-file> [<layer>]\n", argv[0]);
        return 2;
    }

    repos_count = read_lines(argv[1], &repos);
    lines_count = read_lines(argv[2], &lines);
    if (repos_count < 0 || lines_count < 0) {
        return 1;
    }

    for (index = 0; index < repos_count; index++) {
        if (!tig_file_repository_add(repos[index])) {
            fprintf(stderr, "cannot mount %s\n", repos[index]);
            return 1;
        }
    }

    if (argc == 4) {
        snprintf(path, sizeof(path), "%s/TIGCache", argv[3]);
        if (SDL_GetPathInfo(path, NULL)) {
            fprintf(stderr, "refusing %s: mounting it would delete its TIGCache\n", argv[3]);
            return 1;
        }
        if (!tig_file_repository_add(argv[3])) {
            fprintf(stderr, "cannot mount %s\n", argv[3]);
            return 1;
        }
    }

    memset(&init_info, 0, sizeof(init_info));
    // The editor flag only skips the script eye-candy list, which needs art.
    init_info.editor = true;
    if (!obj_harness_init()
        || !script_init(&init_info)
        || !quest_init(&init_info)
        || !rumor_init(&init_info)
        || !dialog_init(&init_info)) {
        fprintf(stderr, "init failed\n");
        return 1;
    }

    if (!self_check()) {
        fprintf(stderr, "self-check failed: the fixture does not hold quests, flags or local flags\n");
        return 1;
    }

    path[0] = '\0';
    for (index = 0; index < lines_count; index++) {
        char* line = lines[index];
        int a;
        int b;

        int rest = 0;

        if (sscanf(line, "case %127s %d %d %d %n", id, &node, &option, &npc_line, &rest) == 4 && rest > 0) {
            snprintf(path, sizeof(path), "%s", line + rest);
            int script_num = atoi(strrchr(path, '/') != NULL ? strrchr(path, '/') + 1 : path);

            reset_world(script_num);
            printf("C\t%s\n", id);
        } else if (sscanf(line, "ss %d", &a) == 1) {
            script_story_state_set(a);
        } else if (sscanf(line, "gf %d %d", &a, &b) == 2) {
            script_global_flag_set(a, b);
        } else if (sscanf(line, "gv %d %d", &a, &b) == 2) {
            script_global_var_set(a, b);
        } else if (sscanf(line, "qu %d %d", &a, &b) == 2) {
            // The engine's own path, so the global state moves with it.
            quest_state_set(pc_obj, a, b, OBJ_HANDLE_NULL);
        } else if (sscanf(line, "lf %d %d", &a, &b) == 2) {
            script_local_flag_set(npc_obj, SAP_DIALOG, a, b);
        } else if (sscanf(line, "stat %d %d", &a, &b) == 2) {
            stat_base_set(pc_obj, a, b);
        } else if (sscanf(line, "skill %d %d", &a, &b) == 2) {
            obj_arrayfield_int32_set(pc_obj, OBJ_F_CRITTER_BASIC_SKILL_IDX, a, b);
        } else if (sscanf(line, "gold %d", &a) == 1) {
            give_item(pc_obj, OBJ_TYPE_GOLD, 0, a);
        } else if (sscanf(line, "item %d", &a) == 1) {
            give_item(pc_obj, OBJ_TYPE_GENERIC, a, 0);
        } else if (sscanf(line, "npcitem %d", &a) == 1) {
            give_item(npc_obj, OBJ_TYPE_GENERIC, a, 0);
        } else if (sscanf(line, "follower %d", &a) == 1) {
            int64_t follower = obj_harness_create_proto(OBJ_TYPE_NPC);
            int count = obj_arrayfield_length_get(pc_obj, OBJ_F_CRITTER_FOLLOWER_IDX);

            obj_field_int32_set(follower, OBJ_F_NAME, a);
            obj_arrayfield_obj_set(pc_obj, OBJ_F_CRITTER_FOLLOWER_IDX, count, follower);
        } else if (sscanf(line, "rumor %d", &a) == 1) {
            // A known rumour is a non-zero timestamp; the harness clock is 0.
            obj_arrayfield_int64_set(pc_obj, OBJ_F_PC_RUMOR_IDX, a - 1000, 1);
        } else if (sscanf(line, "reaction %d", &a) == 1) {
            reaction_adj(npc_obj, pc_obj, a - reaction_get(npc_obj, pc_obj));
        } else if (strcmp(line, "watch ss") == 0 && watch_count < MAX_WATCH) {
            watch_kind[watch_count++] = 's';
        } else if (sscanf(line, "watch gf %d", &a) == 1 && watch_count < MAX_WATCH) {
            watch_kind[watch_count] = 'f';
            watch_num[watch_count++] = a;
        } else if (sscanf(line, "watch gv %d", &a) == 1 && watch_count < MAX_WATCH) {
            watch_kind[watch_count] = 'v';
            watch_num[watch_count++] = a;
        } else if (sscanf(line, "watch qu %d", &a) == 1 && watch_count < MAX_WATCH) {
            watch_kind[watch_count] = 'q';
            watch_num[watch_count++] = a;
        } else if (strcmp(line, "run") == 0) {
            run_case(path, node, option, npc_line);
            fflush(stdout);
        } else if (strcmp(line, "reply") == 0) {
            run_reply(path, node);
            fflush(stdout);
        } else {
            fprintf(stderr, "bad line %d: %s\n", index + 1, line);
            return 1;
        }
    }

    return 0;
}
