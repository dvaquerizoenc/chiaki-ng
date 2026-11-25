// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#include <chiaki/latency.h>
#include <chiaki/time.h>

#include <algorithm>
#include <atomic>

namespace {
struct Samples
{
	std::atomic_flag lock = ATOMIC_FLAG_INIT;
	std::atomic<uint64_t> omitted{0};
	size_t count = 0;
	uint64_t values[CHIAKI_LATENCY_BATCH_CAPACITY];
};

std::atomic<bool> enabled{false};
Samples buffers[CHIAKI_LATENCY_COUNT];
const char *const names[] = {
#define CHIAKI_LATENCY_NAME(name) #name,
	CHIAKI_LATENCY_STAGES(CHIAKI_LATENCY_NAME)
#undef CHIAKI_LATENCY_NAME
};
}

void chiaki_latency_set_enabled(bool value)
{
	enabled.store(value, std::memory_order_relaxed);
}

uint64_t chiaki_latency_begin(void)
{
	return enabled.load(std::memory_order_relaxed) ? chiaki_time_now_monotonic_us() : 0;
}

void chiaki_latency_end(ChiakiLatencyStage stage, uint64_t begin_us)
{
	if(begin_us)
	{
		const uint64_t end_us = chiaki_time_now_monotonic_us();
		if(end_us >= begin_us)
			chiaki_latency_record(stage, end_us - begin_us);
	}
}

void chiaki_latency_record(ChiakiLatencyStage stage, uint64_t duration_us)
{
	if(!enabled.load(std::memory_order_relaxed))
		return;
	auto &buffer = buffers[stage];
	if(buffer.lock.test_and_set(std::memory_order_acquire))
	{
		buffer.omitted.fetch_add(1, std::memory_order_relaxed);
		return;
	}
	if(buffer.count < CHIAKI_LATENCY_BATCH_CAPACITY)
		buffer.values[buffer.count++] = duration_us;
	else
		buffer.omitted.fetch_add(1, std::memory_order_relaxed);
	buffer.lock.clear(std::memory_order_release);
}

const char *chiaki_latency_name(ChiakiLatencyStage stage)
{
	return names[stage];
}

size_t chiaki_latency_drain(ChiakiLatencyStage stage, uint64_t *samples, uint64_t *omitted)
{
	auto &buffer = buffers[stage];
	if(buffer.lock.test_and_set(std::memory_order_acquire))
	{
		*omitted = 0;
		return 0;
	}
	const size_t count = buffer.count;
	std::copy_n(buffer.values, count, samples);
	buffer.count = 0;
	*omitted = buffer.omitted.exchange(0, std::memory_order_relaxed);
	buffer.lock.clear(std::memory_order_release);
	return count;
}

ChiakiLatencySummary chiaki_latency_summarize(uint64_t *samples, size_t count)
{
	if(!count)
		return {};
	std::sort(samples, samples + count);
	uint64_t sum = 0;
	for(size_t i = 0; i < count; i++)
		sum += samples[i];
	const auto percentile = [samples, count](size_t percent) {
		return samples[(percent * count + 99) / 100 - 1];
	};
	return {samples[0], samples[count - 1], sum / count, percentile(50), percentile(95), percentile(99)};
}
