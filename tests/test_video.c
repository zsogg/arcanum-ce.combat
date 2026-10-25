// Tests for `tig/video.c` - the parts of the video layer that run without a
// renderer: buffer blits between SDL surfaces, and the brightness setter's
// bounds.
//
// Written 2026-10-24 for FC1 phase 5f. `Arcanum.exe` is the oracle for each
// case. Pixels are read through `tig_video_buffer_data` under a lock, since
// `tig_video_buffer_get_pixel_color` needs the bit depth only `tig_video_init`
// sets.

#include <string.h>

#include "tig/video.h"

#include "test_harness.h"

static TigVideoBuffer* vb_make(int width, unsigned int background, unsigned int flags, unsigned int color_key)
{
    TigVideoBufferCreateInfo create_info;
    TigVideoBuffer* video_buffer;

    memset(&create_info, 0, sizeof(create_info));
    create_info.flags = flags;
    create_info.width = width;
    create_info.height = 1;
    create_info.background_color = background;
    create_info.color_key = color_key;
    if (tig_video_buffer_create(&create_info, &video_buffer) != TIG_OK) {
        return NULL;
    }
    return video_buffer;
}

static unsigned int vb_pixel(TigVideoBuffer* video_buffer, int x)
{
    TigVideoBufferData data;
    unsigned int pixel;

    tig_video_buffer_lock(video_buffer);
    tig_video_buffer_data(video_buffer, &data);
    pixel = ((unsigned int*)data.pixels)[x];
    tig_video_buffer_unlock(video_buffer);
    return pixel & 0xFFFFFFu;
}

static void vb_blit(TigVideoBuffer* src, TigVideoBuffer* dst, int width, int alpha)
{
    TigVideoBufferBlitInfo blit_info;
    TigRect rect = { 0, 0, width, 1 };

    memset(&blit_info, 0, sizeof(blit_info));
    blit_info.flags = TIG_VIDEO_BUFFER_BLIT_BLEND_ALPHA_CONST;
    blit_info.src_video_buffer = src;
    blit_info.src_rect = &rect;
    blit_info.alpha[0] = (uint8_t)alpha;
    blit_info.dst_video_buffer = dst;
    blit_info.dst_rect = &rect;
    tig_video_buffer_blit(&blit_info);
}

// Porting bug #367: each channel is dst + (src - dst) * alpha / 256, rounded
// down (0x522d08-0x522d84).
TEST(a_constant_alpha_blit_blends_each_channel)
{
    TigVideoBuffer* src = vb_make(2, 0xFF2080, 0, 0);
    TigVideoBuffer* dst = vb_make(2, 0x00FF10, 0, 0);

    CHECK(src != NULL && dst != NULL);
    if (src != NULL && dst != NULL) {
        vb_blit(src, dst, 2, 128);
        // red 0 + 255 * 128 / 256 = 127; green 255 + (-223 * 128 >> 8) = 143;
        // blue 16 + 112 * 128 / 256 = 72.
        CHECK_INT(vb_pixel(dst, 0), 0x7F8F48);
        CHECK_INT(vb_pixel(dst, 1), 0x7F8F48);

        vb_blit(src, dst, 2, 0);
        CHECK_INT(vb_pixel(dst, 0), 0x7F8F48);
    }
    tig_video_buffer_destroy(src);
    tig_video_buffer_destroy(dst);
}

// The colour key is kept: a keyed source pixel leaves the destination alone
// (0x522cfc-0x522d06).
TEST(a_constant_alpha_blit_skips_the_colour_key)
{
    TigVideoBuffer* src = vb_make(1, 0x0000FF, TIG_VIDEO_BUFFER_CREATE_COLOR_KEY, 0x0000FF);
    TigVideoBuffer* dst = vb_make(1, 0x404040, 0, 0);

    CHECK(src != NULL && dst != NULL);
    if (src != NULL && dst != NULL) {
        vb_blit(src, dst, 1, 200);
        CHECK_INT(vb_pixel(dst, 0), 0x404040);
    }
    tig_video_buffer_destroy(src);
    tig_video_buffer_destroy(dst);
}

// Porting bug #366: a gamma not above zero, or from 2.0 up, is refused
// (0x52000a-0x52002a). The set ones need no renderer: the fade they run is
// immediate.
TEST(the_brightness_setter_refuses_what_the_original_refuses)
{
    CHECK_INT(tig_video_set_gamma(0.0f), TIG_ERR_INVALID_PARAM);
    CHECK_INT(tig_video_set_gamma(-0.5f), TIG_ERR_INVALID_PARAM);
    CHECK_INT(tig_video_set_gamma(2.0f), TIG_ERR_INVALID_PARAM);
    CHECK_INT(tig_video_set_gamma(1.5f), TIG_OK);
    CHECK_INT(tig_video_set_gamma(1.0f), TIG_OK);
}

void test_video_suite(void)
{
    RUN(a_constant_alpha_blit_blends_each_channel);
    RUN(a_constant_alpha_blit_skips_the_colour_key);
    RUN(the_brightness_setter_refuses_what_the_original_refuses);
}
