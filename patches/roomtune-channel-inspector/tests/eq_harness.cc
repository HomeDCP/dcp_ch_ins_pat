// 진단 EQ 프리셋 검증 하니스 (헤드리스, wx 비의존)
#include "channel_inspector.h"
#include <cstdio>
#include <cmath>
#include <vector>

static const double FS = 48000.0;
using P = ChannelInspector::Preset;
using G = ChannelInspector::GlobalEq;

// 프리셋+전역 EQ 를 device 채널 test_ch 에 걸고 정상상태 게인(dB) 측정
static double eq_gain_db(P p, G g, double freq, int dev = 2, int ch = 0) {
    ChannelInspector ci;
    ci.configure(8, dev, 4096);
    ci.apply_preset(p);
    ci.set_global_eq(g);
    unsigned N = 24000;
    std::vector<float> buf(N * dev, 0.0f);
    for (unsigned n = 0; n < N; ++n) buf[n*dev + ch] = (float)std::sin(2*M_PI*freq*n/FS);
    ci.eq_process(buf.data(), N);
    double sum = 0; unsigned s = N/2;
    for (unsigned n = s; n < N; ++n) { double v = buf[n*dev + ch]; sum += v*v; }
    double rms = std::sqrt(sum/(N-s));
    return 20*std::log10(rms/std::sqrt(0.5));
}

static int fails = 0;
static void ck(const char* n, bool ok, const char* d="") { std::printf("  [%s] %s %s\n", ok?"PASS":"FAIL", n, d); if(!ok) ++fails; }

int main() {
    std::printf("== 진단 EQ 프리셋 검증 ==\n");

    // None+Off = no-op
    ck("None/Off no-op (1kHz≈0dB)", std::fabs(eq_gain_db(P::None, G::Off, 1000)) < 0.1);

    // DialogueCheck: HP300 + LP3400 + PK2000/+3
    double d100=eq_gain_db(P::DialogueCheck,G::Off,100), d1k=eq_gain_db(P::DialogueCheck,G::Off,1000);
    double d2k=eq_gain_db(P::DialogueCheck,G::Off,2000), d10k=eq_gain_db(P::DialogueCheck,G::Off,10000);
    std::printf("  Dialogue: 100=%.1f 1k=%.1f 2k=%.1f 10k=%.1f dB\n", d100,d1k,d2k,d10k);
    ck("Dialogue HP: 100Hz << 1kHz", d100 < d1k-6);
    ck("Dialogue PK: 2kHz 부스트 > 1kHz", d2k > d1k);
    ck("Dialogue LP: 10kHz << 1kHz", d10k < d1k-6);

    // LfeOnly: LP120 x2
    double l50=eq_gain_db(P::LfeOnly,G::Off,50), l1k=eq_gain_db(P::LfeOnly,G::Off,1000);
    std::printf("  LFE: 50=%.1f 1k=%.1f dB\n", l50,l1k);
    ck("LFE LP: 50Hz 통과, 1kHz 급감", l50 > l1k+18);

    // SurroundAmbience: HP80 + HS8k/+3
    double s40=eq_gain_db(P::SurroundAmbience,G::Off,40), s1k=eq_gain_db(P::SurroundAmbience,G::Off,1000);
    double s14k=eq_gain_db(P::SurroundAmbience,G::Off,14000);
    std::printf("  Surround: 40=%.1f 1k=%.1f 14k=%.1f dB\n", s40,s1k,s14k);
    ck("Surround HP: 40Hz << 1kHz", s40 < s1k-6);
    ck("Surround HS: 14kHz 부스트", s14k > s1k+1);

    // HfIntegrity: HP8k
    double h1k=eq_gain_db(P::HfIntegrity,G::Off,1000), h12k=eq_gain_db(P::HfIntegrity,G::Off,12000);
    std::printf("  HF: 1k=%.1f 12k=%.1f dB\n", h1k,h12k);
    ck("HF HP: 1kHz << 12kHz", h1k < h12k-6);

    // XCurve global: 저역 평탄, 고역 감쇠
    double x500=eq_gain_db(P::None,G::XCurve,500), x10k=eq_gain_db(P::None,G::XCurve,10000);
    std::printf("  XCurve: 500=%.1f 10k=%.1f dB\n", x500,x10k);
    ck("XCurve: 저역 평탄(500Hz≈0)", std::fabs(x500) < 1.5);
    ck("XCurve: 고역 감쇠(10kHz<-4)", x10k < -4);

    // 독립 적용: Dialogue + XCurve 동시
    double dx10k=eq_gain_db(P::DialogueCheck,G::XCurve,10000), donly=eq_gain_db(P::DialogueCheck,G::Off,10000);
    std::printf("  Dialogue+XCurve 10k=%.1f (Dialogue만=%.1f)\n", dx10k,donly);
    ck("독립적용: XCurve 추가 감쇠", dx10k < donly-2);

    // 자동 solo 설정
    {
        ChannelInspector ci; ci.configure(8, 2, 512);
        ci.apply_preset(P::DialogueCheck);
        ck("Dialogue solo=C(2)", ci.solo(2) && ci.has_solo() && !ci.solo(0));
        ci.apply_preset(P::SurroundAmbience);
        ck("Surround solo=Ls(4)+Rs(5)", ci.solo(4) && ci.solo(5) && !ci.solo(2));
        ci.apply_preset(P::HfIntegrity);
        ck("HF: solo 없음(전체 믹스)", !ci.has_solo());
        ci.apply_preset(P::None);
        ck("None: solo 전부 해제", !ci.has_solo());
    }

    std::printf("\n== %s (%d fail) ==\n", fails==0?"ALL PASS":"FAIL", fails);
    return fails==0 ? 0 : 1;
}
