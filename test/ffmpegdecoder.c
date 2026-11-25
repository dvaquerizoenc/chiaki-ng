// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <munit.h>

#include <chiaki/ffmpegdecoder.h>

#include <libavutil/frame.h>
#include <libavutil/avutil.h>
#include <libavutil/mem.h>
#include <limits.h>

/* Helper: allocate an AVFrame with both timestamp fields set to AV_NOPTS_VALUE */
static AVFrame *alloc_blank_frame(void)
{
	AVFrame *f = av_frame_alloc();
	munit_assert_not_null(f);
	f->best_effort_timestamp = AV_NOPTS_VALUE;
	f->pts                   = AV_NOPTS_VALUE;
	return f;
}

/* pts = best_effort_timestamp * av_q2d(pkt_timebase) */
static MunitResult test_pts_from_best_effort(const MunitParameter params[], void *user)
{
	AVFrame *frame = alloc_blank_frame();
	frame->best_effort_timestamp = 12345;

	double pts = 0.0, dur = 0.0;
	/* 1/90000 timebase, 60 fps */
	chiaki_ffmpeg_frame_get_timing(
		frame,
		(AVRational){1, 90000},  /* pkt_timebase */
		(AVRational){0, 0},      /* ctx_timebase (invalid → not used) */
		(AVRational){60, 1},     /* framerate */
		&pts, &dur);

	munit_assert_double_equal(pts, 12345.0 / 90000.0, 9);
	munit_assert_double_equal(dur, 1.0 / 60.0, 9);

	av_frame_free(&frame);
	return MUNIT_OK;
}

/* best_effort_timestamp == AV_NOPTS_VALUE → fall back to frame->pts */
static MunitResult test_pts_fallback_to_pts(const MunitParameter params[], void *user)
{
	AVFrame *frame = alloc_blank_frame();
	frame->pts = 9000;  /* best_effort stays NOPTS */

	double pts = 0.0, dur = 0.0;
	chiaki_ffmpeg_frame_get_timing(
		frame,
		(AVRational){1, 90000},
		(AVRational){0, 0},
		(AVRational){30, 1},
		&pts, &dur);

	munit_assert_double_equal(pts, 9000.0 / 90000.0, 9);
	munit_assert_double_equal(dur, 1.0 / 30.0, 9);

	av_frame_free(&frame);
	return MUNIT_OK;
}

/* both timestamp fields == AV_NOPTS_VALUE → pts == 0.0 */
static MunitResult test_pts_both_nopts(const MunitParameter params[], void *user)
{
	AVFrame *frame = alloc_blank_frame();
	/* both stay AV_NOPTS_VALUE */

	double pts = -1.0, dur = 0.0;
	chiaki_ffmpeg_frame_get_timing(
		frame,
		(AVRational){1, 90000},
		(AVRational){0, 0},
		(AVRational){60, 1},
		&pts, &dur);

	munit_assert_double_equal(pts, 0.0, 9);

	av_frame_free(&frame);
	return MUNIT_OK;
}

/* pkt_timebase invalid → fall through to ctx_timebase */
static MunitResult test_timebase_fallback_to_ctx(const MunitParameter params[], void *user)
{
	AVFrame *frame = alloc_blank_frame();
	frame->best_effort_timestamp = 1000;

	double pts = 0.0, dur = 0.0;
	/* pkt_timebase invalid, ctx_timebase = 1/1000 */
	chiaki_ffmpeg_frame_get_timing(
		frame,
		(AVRational){0, 0},    /* invalid pkt_timebase */
		(AVRational){1, 1000}, /* ctx_timebase */
		(AVRational){60, 1},
		&pts, &dur);

	munit_assert_double_equal(pts, 1000.0 / 1000.0, 9);

	av_frame_free(&frame);
	return MUNIT_OK;
}

/* both timebases invalid → fall back to 1/1000000 (microsecond timebase) */
static MunitResult test_timebase_fallback_to_default(const MunitParameter params[], void *user)
{
	AVFrame *frame = alloc_blank_frame();
	frame->best_effort_timestamp = 1000000; /* 1 second in µs */

	double pts = 0.0, dur = 0.0;
	chiaki_ffmpeg_frame_get_timing(
		frame,
		(AVRational){0, 0},  /* invalid */
		(AVRational){0, 0},  /* invalid */
		(AVRational){60, 1},
		&pts, &dur);

	munit_assert_double_equal(pts, 1.0, 9);

	av_frame_free(&frame);
	return MUNIT_OK;
}

/* framerate invalid (0/0) → fps falls back to 60, duration = 1/60 */
static MunitResult test_duration_framerate_fallback(const MunitParameter params[], void *user)
{
	AVFrame *frame = alloc_blank_frame();
	frame->best_effort_timestamp = 0;

	double pts = 0.0, dur = 0.0;
	chiaki_ffmpeg_frame_get_timing(
		frame,
		(AVRational){1, 90000},
		(AVRational){0, 0},
		(AVRational){0, 0},  /* invalid framerate */
		&pts, &dur);

	munit_assert_double_equal(dur, 1.0 / 60.0, 9);

	av_frame_free(&frame);
	return MUNIT_OK;
}

/* 120 fps → duration = 1/120 */
static MunitResult test_duration_120fps(const MunitParameter params[], void *user)
{
	AVFrame *frame = alloc_blank_frame();
	frame->best_effort_timestamp = 0;

	double pts = 0.0, dur = 0.0;
	chiaki_ffmpeg_frame_get_timing(
		frame,
		(AVRational){1, 90000},
		(AVRational){0, 0},
		(AVRational){120, 1},
		&pts, &dur);

	munit_assert_double_equal(dur, 1.0 / 120.0, 9);

	av_frame_free(&frame);
	return MUNIT_OK;
}

static MunitResult test_packet_allocation_failure(const MunitParameter params[], void *user)
{
	ChiakiFfmpegDecoder decoder = {0};
	munit_assert_int(chiaki_ffmpeg_decoder_init(&decoder, NULL, CHIAKI_CODEC_H264, 60,
		NULL, NULL, NULL, NULL), ==, CHIAKI_ERR_SUCCESS);
	uint8_t sample = 0;
	av_max_alloc(1);
	const bool success = chiaki_ffmpeg_decoder_video_sample_cb(&sample, sizeof(sample), 1, true, &decoder);
	av_max_alloc(INT_MAX);
	munit_assert_false(success);
	munit_assert_int(decoder.frames_lost, ==, 0);
	munit_assert_false(decoder.frame_recovered);
	chiaki_ffmpeg_decoder_fini(&decoder);
	return MUNIT_OK;
}

MunitTest tests_ffmpegdecoder[] = {
	{ "/packet_allocation_failure", test_packet_allocation_failure,
		NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{
		"/pts_from_best_effort",
		test_pts_from_best_effort,
		NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
	},
	{
		"/pts_fallback_to_pts",
		test_pts_fallback_to_pts,
		NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
	},
	{
		"/pts_both_nopts",
		test_pts_both_nopts,
		NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
	},
	{
		"/timebase_fallback_to_ctx",
		test_timebase_fallback_to_ctx,
		NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
	},
	{
		"/timebase_fallback_to_default",
		test_timebase_fallback_to_default,
		NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
	},
	{
		"/duration_framerate_fallback",
		test_duration_framerate_fallback,
		NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
	},
	{
		"/duration_120fps",
		test_duration_120fps,
		NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
	},
	{ NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
