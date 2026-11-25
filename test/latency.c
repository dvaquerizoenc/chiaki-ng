// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <chiaki/latency.h>
#include <munit.h>

static MunitResult test_independent_stages(const MunitParameter params[], void *user)
{
	uint64_t samples[CHIAKI_LATENCY_BATCH_CAPACITY];
	uint64_t omitted = 0;
	chiaki_latency_set_enabled(true);
	for(uint64_t i = 100; i; i--)
		chiaki_latency_record(CHIAKI_LATENCY_submit_call, i);
	chiaki_latency_record(CHIAKI_LATENCY_swap_call, 900);
	chiaki_latency_set_enabled(false);
	chiaki_latency_record(CHIAKI_LATENCY_swap_call, 800);
	munit_assert_size(chiaki_latency_drain(CHIAKI_LATENCY_submit_call, samples, &omitted), ==, 100);
	munit_assert_uint64(omitted, ==, 0);
	const ChiakiLatencySummary stats = chiaki_latency_summarize(samples, 100);
	munit_assert_uint64(stats.p50_us, ==, 50);
	munit_assert_uint64(stats.p95_us, ==, 95);
	munit_assert_uint64(stats.p99_us, ==, 99);
	munit_assert_size(chiaki_latency_drain(CHIAKI_LATENCY_submit_call, samples, &omitted), ==, 0);
	munit_assert_size(chiaki_latency_drain(CHIAKI_LATENCY_swap_call, samples, &omitted), ==, 1);
	munit_assert_uint64(samples[0], ==, 900);
	return MUNIT_OK;
}

static MunitResult test_omitted_samples(const MunitParameter params[], void *user)
{
	uint64_t samples[CHIAKI_LATENCY_BATCH_CAPACITY];
	uint64_t omitted = 0;
	chiaki_latency_set_enabled(true);
	for(size_t i = 0; i < CHIAKI_LATENCY_BATCH_CAPACITY + 2; i++)
		chiaki_latency_record(CHIAKI_LATENCY_mouse_input, i);
	chiaki_latency_set_enabled(false);
	munit_assert_size(chiaki_latency_drain(CHIAKI_LATENCY_mouse_input, samples, &omitted), ==, CHIAKI_LATENCY_BATCH_CAPACITY);
	munit_assert_uint64(omitted, ==, 2);
	munit_assert_size(chiaki_latency_drain(CHIAKI_LATENCY_mouse_input, samples, &omitted), ==, 0);
	munit_assert_uint64(omitted, ==, 0);
	return MUNIT_OK;
}

MunitTest tests_latency[] = {
	{ "/independent_stages", test_independent_stages, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ "/omitted_samples", test_omitted_samples, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
