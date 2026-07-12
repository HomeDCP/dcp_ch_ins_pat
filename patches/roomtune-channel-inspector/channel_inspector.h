/*
    RoomTune — DCP 검수용 오디오 채널 인스펙터 (실시간 엔진, 비-UI)
    DCP-o-matic 파생물 (GPL-2.0-or-later).

    DCP 재생 검수 시 채널 매핑을 실측 검증하는 엔진.
      • 채널별 solo/mute — 개별 채널을 격리/소거하여 매핑 확인
      • 채널별 피크 미터 — 각 DCP 채널에 실제 신호가 오는지 확인
      • 무할당 다운믹스 매트릭스 — DCP 레이아웃(N ch) → 출력 장치(dev ch) 라우팅

    v1 범위: 위 3가지(채널 검수 코어). 진단 EQ 프리셋은 다음 반복(biquad.h 공유 예정).

    스레드 모델(SPSC):
      • GUI(단일 writer): configure/set_solo/set_mute/clear/publish
      • RtAudio 콜백(단일 reader): mid_buffer/meter/downmix
      라우팅 매트릭스는 더블버퍼 + atomic 인덱스로 lock-free 발행한다.
      콜백 경로(meter/downmix)는 힙 할당·락·예외 없음(noexcept).
*/

#ifndef DCPOMATIC_CHANNEL_INSPECTOR_H
#define DCPOMATIC_CHANNEL_INSPECTOR_H

#include <atomic>
#include <vector>
#include <cstddef>
#include <cmath>
#include <algorithm>

class ChannelInspector
{
public:
	static int const MAX_DCP = 16;   /* DCP 오디오 채널 상한(MAX_DCP_AUDIO_CHANNELS) */
	static int const MAX_DEV = 16;   /* 출력 장치 채널 상한 */

	ChannelInspector() {
		for (int i = 0; i < MAX_DCP; ++i) _peak[i].store(0.0f, std::memory_order_relaxed);
		zero_matrix(_matrix[0]);
		zero_matrix(_matrix[1]);
	}

	/** 라우팅 게인 매트릭스: g[dcp_src][dev_out]. trivially copyable(atomic 아님). */
	struct MatrixSrc { float g[MAX_DCP][MAX_DEV]; };

	/* ───────── 구성 (GUI 스레드, 콜백 suspend 중에만) ───────── */

	/** 중간버퍼 재할당 + 상태 리셋. dcp_channels=DCP 채널수, dev_channels=출력장치 채널수. */
	void configure(int dcp_channels, int dev_channels, unsigned int block_size) {
		_dcp_ch = std::max(0, std::min(dcp_channels, MAX_DCP));
		_dev_ch = std::max(0, std::min(dev_channels, MAX_DEV));
		_block  = block_size;
		_mid.assign(static_cast<std::size_t>(block_size) * MAX_DCP, 0.0f);
		for (int i = 0; i < MAX_DCP; ++i) _peak[i].store(0.0f, std::memory_order_relaxed);
	}

	bool active_pipeline() const { return _dcp_ch > 0 && _dev_ch > 0 && !_mid.empty(); }
	int  dcp_channels() const { return _dcp_ch; }
	int  dev_channels() const { return _dev_ch; }

	/* ───────── 콜백 (오디오 스레드, 무할당·noexcept) ───────── */

	/** DCP 레이아웃 오디오를 담을 사전할당 중간버퍼(≥ frames*MAX_DCP). */
	float* mid_buffer(unsigned int /*frames*/) noexcept { return _mid.data(); }

	/** 채널별 피크(다운믹스/뮤트 이전, 원신호 기준)를 atomic 으로 기록. */
	void meter(float const* mid, unsigned int frames) noexcept {
		int const dcp = _dcp_ch;
		for (int c = 0; c < dcp; ++c) {
			float pk = 0.0f;
			for (unsigned int f = 0; f < frames; ++f) {
				float const v = std::fabs(mid[static_cast<std::size_t>(f) * dcp + c]);
				if (v > pk) pk = v;
			}
			_peak[c].store(pk, std::memory_order_relaxed);
		}
	}

	/** 무할당 다운믹스: mid(dcp ch 인터리브) → out(dev ch 인터리브), M 스냅샷 적용. */
	void downmix(float const* mid, float* out, unsigned int frames) const noexcept {
		int const idx = _pub.load(std::memory_order_acquire);
		MatrixSrc const& M = _matrix[idx];
		int const dcp = _dcp_ch, dev = _dev_ch;
		for (unsigned int f = 0; f < frames; ++f) {
			float const* in = mid + static_cast<std::size_t>(f) * dcp;
			float* o = out + static_cast<std::size_t>(f) * dev;
			for (int j = 0; j < dev; ++j) o[j] = 0.0f;
			for (int c = 0; c < dcp; ++c) {
				float const s = in[c];
				float const* row = M.g[c];
				for (int j = 0; j < dev; ++j) {
					float const g = row[j];
					if (g != 0.0f) o[j] += s * g;
				}
			}
		}
	}

	/* ───────── 상태 (GUI 스레드) ───────── */

	void set_solo(int c, bool on) { if (c >= 0 && c < MAX_DCP) _solo[c] = on; }
	void set_mute(int c, bool on) { if (c >= 0 && c < MAX_DCP) _mute[c] = on; }
	bool solo(int c) const { return c >= 0 && c < MAX_DCP && _solo[c]; }
	bool mute(int c) const { return c >= 0 && c < MAX_DCP && _mute[c]; }
	bool has_solo() const { for (int i = 0; i < MAX_DCP; ++i) if (_solo[i]) return true; return false; }
	void clear() { for (int i = 0; i < MAX_DCP; ++i) { _solo[i] = false; _mute[i] = false; } }

	/** 라우팅 매트릭스를 lock-free 로 발행(비활성 버퍼 기록 후 인덱스 플립). */
	void publish(MatrixSrc const& src) {
		int const w = 1 - _pub.load(std::memory_order_relaxed);
		_matrix[w] = src;
		_pub.store(w, std::memory_order_release);
	}

	/** 채널 피크(dBFS). 무신호는 -120 dB. */
	float peak_dbfs(int c) const {
		if (c < 0 || c >= MAX_DCP) return -120.0f;
		float const p = _peak[c].load(std::memory_order_relaxed);
		return p > 1e-6f ? 20.0f * std::log10(p) : -120.0f;
	}

private:
	static void zero_matrix(MatrixSrc& m) {
		for (int c = 0; c < MAX_DCP; ++c)
			for (int j = 0; j < MAX_DEV; ++j) m.g[c][j] = 0.0f;
	}

	int _dcp_ch = 0, _dev_ch = 0;
	unsigned int _block = 0;
	std::vector<float> _mid;                  /* block*MAX_DCP 사전할당 */
	std::atomic<float> _peak[MAX_DCP];
	MatrixSrc _matrix[2];
	std::atomic<int> _pub{0};                 /* 콜백이 읽는 활성 인덱스 */
	bool _solo[MAX_DCP] = {};
	bool _mute[MAX_DCP] = {};
};

#endif
