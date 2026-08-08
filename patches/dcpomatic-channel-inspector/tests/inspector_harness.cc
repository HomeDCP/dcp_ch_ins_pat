#include "channel_inspector.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>


static int failures = 0;


static void check(char const* name, bool ok)
{
	std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
	if (!ok) {
		++failures;
	}
}


int main()
{
	ChannelInspector inspector;
	inspector.configure(6, 2, 512);

	check("active pipeline", inspector.active_pipeline());
	check("can process configured block", inspector.can_process(512));
	check("reject oversized block", !inspector.can_process(513));
	check("dcp channel count", inspector.dcp_channels() == 6);
	check("device channel count", inspector.device_channels() == 2);

	ChannelInspector::Matrix matrix;
	for (auto& row: matrix.gain) {
		for (auto& gain: row) {
			gain = 0.0f;
		}
	}
	matrix.gain[0][0] = 1.0f;
	matrix.gain[1][1] = 1.0f;
	matrix.gain[2][0] = 0.7f;
	matrix.gain[2][1] = 0.7f;
	inspector.publish(matrix);

	auto const frames = 8U;
	std::vector<float> mid(frames * 6, 0.0f);
	std::vector<float> out(frames * 2, -99.0f);
	for (auto f = 0U; f < frames; ++f) {
		mid[f * 6 + 0] = 1.0f + static_cast<float>(f);
		mid[f * 6 + 1] = 2.0f + static_cast<float>(f);
		mid[f * 6 + 2] = 0.5f;
		mid[f * 6 + 3] = 9.0f;
	}

	inspector.downmix(mid.data(), out.data(), frames);
	bool downmix_ok = true;
	for (auto f = 0U; f < frames; ++f) {
		auto const expected_l = (1.0f + static_cast<float>(f)) + 0.5f * 0.7f;
		auto const expected_r = (2.0f + static_cast<float>(f)) + 0.5f * 0.7f;
		downmix_ok = downmix_ok && std::fabs(out[f * 2] - expected_l) < 1e-5f;
		downmix_ok = downmix_ok && std::fabs(out[f * 2 + 1] - expected_r) < 1e-5f;
	}
	check("downmix applies published matrix", downmix_ok);

	inspector.meter(mid.data(), frames);
	check("peak ch0 approximately 18.1 dBFS", std::fabs(inspector.peak_dbfs(0) - 18.1f) < 0.5f);
	check("peak ch3 approximately 19.1 dBFS", std::fabs(inspector.peak_dbfs(3) - 19.1f) < 0.5f);
	check("silent channel is -inf bucket", inspector.peak_dbfs(4) < -100.0f);

	check("initially no solo", !inspector.has_solo());
	inspector.set_solo(2, true);
	inspector.set_mute(4, true);
	check("solo state", inspector.has_solo() && inspector.solo(2));
	check("mute state", inspector.mute(4));
	inspector.clear();
	check("clear resets solo and mute", !inspector.has_solo() && !inspector.mute(4));

	ChannelInspector wide;
	wide.configure(2, 18, 4);
	check("wide device clamps monitor columns", wide.device_channels() == ChannelInspector::MAX_DEV);

	ChannelInspector::Matrix wide_matrix;
	for (auto& row: wide_matrix.gain) {
		for (auto& gain: row) {
			gain = 0.0f;
		}
	}
	wide_matrix.gain[0][0] = 1.0f;
	wide_matrix.gain[1][15] = 1.0f;
	wide.publish(wide_matrix);

	std::vector<float> wide_mid(2 * 2, 0.0f);
	std::vector<float> wide_out(2 * 18, -99.0f);
	wide_mid[0] = 1.0f;
	wide_mid[1] = 2.0f;
	wide_mid[2] = 3.0f;
	wide_mid[3] = 4.0f;
	wide.downmix(wide_mid.data(), wide_out.data(), 2);

	bool wide_stride_ok = true;
	for (int frame = 0; frame < 2; ++frame) {
		auto const base = frame * 18;
		wide_stride_ok = wide_stride_ok && std::fabs(wide_out[base] - wide_mid[frame * 2]) < 1e-5f;
		wide_stride_ok = wide_stride_ok && std::fabs(wide_out[base + 15] - wide_mid[frame * 2 + 1]) < 1e-5f;
		for (int channel = 1; channel < 18; ++channel) {
			if (channel != 15) {
				wide_stride_ok = wide_stride_ok && std::fabs(wide_out[base + channel]) < 1e-5f;
			}
		}
	}
	check("wide device uses actual output stride", wide_stride_ok);

	std::printf("\n== %s (%d failure%s) ==\n", failures == 0 ? "ALL PASS" : "FAIL", failures, failures == 1 ? "" : "s");
	return failures == 0 ? 0 : 1;
}
