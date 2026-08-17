// Tests for malformed object-file and object-buffer reads.
//
// These close the two documented untrusted-file items: `obj_proto_read_file`
// indexing `object_fields_count_per_type` with a stream-supplied type, and
// `obj_inst_read_file` CALLOCing from a stream-supplied field count. The file
// and memory paths share the same checks, so both are exercised here.
//
// Fixtures are raw bytes, not real objects: the reader should reject them
// before any field data is consumed, so no valid object needs to be built.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "game/obj.h"
#include "game/obj_file.h"

#include "obj_harness.h"
#include "test_harness.h"

#define FIXTURE_PROTO_PATH "./arcanum_test_malformed.pro"
#define FIXTURE_INST_PATH "./arcanum_test_malformed.mob"

static void put_i16(uint8_t* dst, int16_t value)
{
    memcpy(dst, &value, sizeof(value));
}

static void put_i32(uint8_t* dst, int32_t value)
{
    memcpy(dst, &value, sizeof(value));
}

static bool write_fixture(const char* path, const uint8_t* data, size_t size)
{
    FILE* stream = fopen(path, "wb");

    if (stream == NULL) {
        return false;
    }

    fwrite(data, 1, size, stream);
    fclose(stream);

    return true;
}

static void remove_fixture(const char* path)
{
    remove(path);
}

// version(119) + prototype OID + object OID + type. The type is out of range
// for the field-count table, so the reader must reject it before the bitmap.
static void make_proto_type_fixture(uint8_t* data, int type)
{
    memset(data, 0, 56);

    put_i32(data + 0, 119);
    put_i16(data + 4, OID_TYPE_BLOCKED);
    put_i32(data + 52, type);
}

// version(119) + instance OID (not blocked) + object OID + type + num_fields.
// The field count is stream-controlled; the negative value below is the
// smallest request the reader must refuse.
static void make_inst_field_count_fixture(uint8_t* data, int16_t num_fields)
{
    memset(data, 0, 58);

    put_i32(data + 0, 119);
    put_i16(data + 4, OID_TYPE_A);
    put_i32(data + 52, OBJ_TYPE_NPC);
    put_i16(data + 56, num_fields);
}

static void make_inst_type_fixture(uint8_t* data, int type)
{
    memset(data, 0, 58);

    put_i32(data + 0, 119);
    put_i16(data + 4, OID_TYPE_A);
    put_i32(data + 52, type);
    put_i16(data + 56, 0);
}

TEST(memory_reader_rejects_prototype_type_out_of_range)
{
    uint8_t data[56];
    int64_t obj;

    CHECK(obj_harness_init());

    make_proto_type_fixture(data, 18);
    obj = OBJ_HANDLE_NULL;
    CHECK(!obj_read_mem(data, &obj));
    CHECK(obj == OBJ_HANDLE_NULL);

    make_proto_type_fixture(data, -1);
    obj = OBJ_HANDLE_NULL;
    CHECK(!obj_read_mem(data, &obj));
    CHECK(obj == OBJ_HANDLE_NULL);
}

TEST(memory_reader_rejects_instance_negative_field_count)
{
    uint8_t data[58];
    int64_t obj;

    CHECK(obj_harness_init());

    make_inst_field_count_fixture(data, -1);
    obj = OBJ_HANDLE_NULL;
    CHECK(!obj_read_mem(data, &obj));
    CHECK(obj == OBJ_HANDLE_NULL);
}

TEST(memory_reader_rejects_instance_type_out_of_range)
{
    uint8_t data[58];
    int64_t obj;

    CHECK(obj_harness_init());

    make_inst_type_fixture(data, 18);
    obj = OBJ_HANDLE_NULL;
    CHECK(!obj_read_mem(data, &obj));
    CHECK(obj == OBJ_HANDLE_NULL);
}

TEST(file_reader_rejects_prototype_type_out_of_range)
{
    uint8_t data[56];
    int64_t obj;

    CHECK(obj_harness_init());

    make_proto_type_fixture(data, 18);
    CHECK(write_fixture(FIXTURE_PROTO_PATH, data, sizeof(data)));

    obj = OBJ_HANDLE_NULL;
    CHECK(!objf_solitary_read(&obj, FIXTURE_PROTO_PATH));
    CHECK(obj == OBJ_HANDLE_NULL);

    remove_fixture(FIXTURE_PROTO_PATH);
}

TEST(file_reader_rejects_instance_negative_field_count)
{
    uint8_t data[58];
    int64_t obj;

    CHECK(obj_harness_init());

    make_inst_field_count_fixture(data, -1);
    CHECK(write_fixture(FIXTURE_INST_PATH, data, sizeof(data)));

    obj = OBJ_HANDLE_NULL;
    CHECK(!objf_solitary_read(&obj, FIXTURE_INST_PATH));
    CHECK(obj == OBJ_HANDLE_NULL);

    remove_fixture(FIXTURE_INST_PATH);
}

void test_obj_file_suite(void)
{
    memory_reader_rejects_prototype_type_out_of_range();
    memory_reader_rejects_instance_negative_field_count();
    memory_reader_rejects_instance_type_out_of_range();
    file_reader_rejects_prototype_type_out_of_range();
    file_reader_rejects_instance_negative_field_count();
}
