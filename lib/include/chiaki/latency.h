// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#ifndef CHIAKI_LATENCY_H
#define CHIAKI_LATENCY_H

#include <chiaki/common.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHIAKI_LATENCY_STAGES(X) \
	X(schedule_to_render) \
	X(render_to_swap) \
	X(pl_queue_push) \
	X(pending_apply_queueStoredFrame) \
	X(pending_apply) \
	X(submit_queue_delay) \
	X(submit_pacing_wait) \
	X(pl_swapchain_submit_frame) \
	X(submit_call) \
	X(submit_frame_block) \
	X(submit_to_swap) \
	X(present_request_to_swap_return) \
	X(swap_call) \
	X(render_dispatch_to_entry) \
	X(render_idle_to_entry) \
	X(pending_release_wait_to_render) \
	X(start_frame_block) \
	X(render_to_start_frame) \
	X(start_frame_to_submit) \
	X(render_prep) \
	X(render_image) \
	X(render_work) \
	X(mouse_input) \
	X(mouse_to_state) \
	X(udp_send) \
	X(udp_send_failed) \
	X(feedback_to_udp) \
	X(decode_send_wait) \
	X(decode_receive_wait) \
	X(decode_send) \
	X(decode_receive) \
	X(prepare_frame) \
	X(ram_transfer) \
	X(snapshot_ram_transfer) \
	X(frame_age_at_map) \
	X(map_frame) \
	X(gui_handoff)

typedef enum
{
#define CHIAKI_LATENCY_ENUM(name) CHIAKI_LATENCY_##name,
	CHIAKI_LATENCY_STAGES(CHIAKI_LATENCY_ENUM)
#undef CHIAKI_LATENCY_ENUM
	CHIAKI_LATENCY_COUNT
} ChiakiLatencyStage;

#define CHIAKI_LATENCY_BATCH_CAPACITY 4096

typedef struct
{
	uint64_t min_us, max_us, avg_us;
	uint64_t p50_us, p95_us, p99_us;
} ChiakiLatencySummary;

CHIAKI_EXPORT void chiaki_latency_set_enabled(bool enabled);
CHIAKI_EXPORT uint64_t chiaki_latency_begin(void);
CHIAKI_EXPORT void chiaki_latency_end(ChiakiLatencyStage stage, uint64_t begin_us);
CHIAKI_EXPORT void chiaki_latency_record(ChiakiLatencyStage stage, uint64_t duration_us);
CHIAKI_EXPORT const char *chiaki_latency_name(ChiakiLatencyStage stage);
// The consumer supplies space for CHIAKI_LATENCY_BATCH_CAPACITY samples.
CHIAKI_EXPORT size_t chiaki_latency_drain(ChiakiLatencyStage stage, uint64_t *samples, uint64_t *omitted);
CHIAKI_EXPORT ChiakiLatencySummary chiaki_latency_summarize(uint64_t *samples, size_t count);

#ifdef __cplusplus
}
#endif
#endif
