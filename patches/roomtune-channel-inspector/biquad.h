/*
    RoomTune — 공유 biquad DSP 코어 (RBJ Audio EQ Cookbook)
    DCP-o-matic 파생물 (GPL-2.0-or-later).

    room_eq.h(재생 룸튜닝 EQ)와 channel_inspector.h(검수용 채널 인스펙터)가
    공유하는 순수 DSP 코어. wx/DCP-o-matic 의존 없음 → 헤드리스 단위테스트 가능.

    계수는 모두 Q 로 매개변수화(REW / EQ APO 파라메트릭과 일치). 부스트 상한·프리앰프
    등 정책 규칙은 각 소비자(room_eq.h 등)가 적용한다 — 이 파일은 순수 필터 계수만.
*/

#ifndef DCPOMATIC_BIQUAD_H
#define DCPOMATIC_BIQUAD_H

#include <cmath>

namespace roomtune {

/** biquad 상태(채널·밴드별로 하나씩). */
struct BiquadState { double z1 = 0; double z2 = 0; };

/** Transposed Direct Form II biquad (정규화된 계수, a0 = 1). */
struct Biquad {
	double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
	inline double process(BiquadState& s, double x) const {
		double const y = b0 * x + s.z1;
		s.z1 = b1 * x - a1 * y + s.z2;
		s.z2 = b2 * x - a2 * y;
		return y;
	}
};

inline Biquad normalise(double b0, double b1, double b2, double a0, double a1, double a2) {
	Biquad q;
	q.b0 = b0 / a0; q.b1 = b1 / a0; q.b2 = b2 / a0;
	q.a1 = a1 / a0; q.a2 = a2 / a0;
	return q;
}

/* RBJ peaking EQ (Q 기반 — REW/EQ APO 파라메트릭과 일치) */
inline Biquad peaking(double fc, double Q, double gain_db, double fs) {
	double const A = std::pow(10.0, gain_db / 40.0);
	double const w0 = 2.0 * M_PI * fc / fs;
	double const cw = std::cos(w0), sw = std::sin(w0);
	double const al = sw / (2.0 * Q);
	return normalise(1 + al * A, -2 * cw, 1 - al * A,
	                 1 + al / A, -2 * cw, 1 - al / A);
}

/* Q 로 매개변수화한 RBJ 셸프 (low=true → low-shelf, false → high-shelf) */
inline Biquad shelf(double fc, double Q, double gain_db, double fs, bool low) {
	double const A = std::pow(10.0, gain_db / 40.0);
	double const w0 = 2.0 * M_PI * fc / fs;
	double const cw = std::cos(w0), sw = std::sin(w0);
	double const al = sw / (2.0 * Q);
	double const t = 2.0 * std::sqrt(A) * al;
	if (low) {
		return normalise(
			A * ((A + 1) - (A - 1) * cw + t),
			2 * A * ((A - 1) - (A + 1) * cw),
			A * ((A + 1) - (A - 1) * cw - t),
			(A + 1) + (A - 1) * cw + t,
			-2 * ((A - 1) + (A + 1) * cw),
			(A + 1) + (A - 1) * cw - t);
	}
	return normalise(
		A * ((A + 1) + (A - 1) * cw + t),
		-2 * A * ((A - 1) + (A + 1) * cw),
		A * ((A + 1) + (A - 1) * cw - t),
		(A + 1) - (A - 1) * cw + t,
		2 * ((A - 1) - (A + 1) * cw),
		(A + 1) - (A - 1) * cw - t);
}

/* RBJ 2차 LPF/HPF (Q 기반) */
inline Biquad pass(double fc, double Q, double fs, bool low) {
	double const w0 = 2.0 * M_PI * fc / fs;
	double const cw = std::cos(w0), sw = std::sin(w0);
	double const al = sw / (2.0 * Q);
	if (low) {
		return normalise((1 - cw) / 2, 1 - cw, (1 - cw) / 2,
		                 1 + al, -2 * cw, 1 - al);
	}
	return normalise((1 + cw) / 2, -(1 + cw), (1 + cw) / 2,
	                 1 + al, -2 * cw, 1 - al);
}

} // namespace roomtune

#endif
