// RoomEQ 파서+DSP 재검증 하니스 (헤드리스, 의존성 없음)
#include "room_eq.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <cstdlib>

static const double FS = 48000.0;

// test_ch 에 주파수 freq 사인파를 넣고 정상상태 게인(dB)을 측정 (preamp 포함 총 게인)
static double gain_db(RoomEQ& eq, double freq, unsigned ch_count, unsigned test_ch) {
    unsigned N = 48000; // 1초
    std::vector<float> buf(N * ch_count, 0.0f);
    for (unsigned n = 0; n < N; ++n)
        buf[(size_t)n*ch_count + test_ch] = (float)std::sin(2*M_PI*freq*n/FS);
    eq.process(buf.data(), N, ch_count);
    double sum = 0; unsigned start = N*3/4; // 뒷 1/4 = 정상상태
    for (unsigned n = start; n < N; ++n) {
        double v = buf[(size_t)n*ch_count + test_ch];
        sum += v*v;
    }
    double rms = std::sqrt(sum/(N-start));
    return 20*std::log10(rms / std::sqrt(0.5)); // 입력 사인 RMS = 1/sqrt(2)
}

static int fails = 0;
static void check(const char* name, bool ok, const char* detail="") {
    std::printf("  [%s] %s %s\n", ok?"PASS":"FAIL", name, detail);
    if (!ok) ++fails;
}

int main() {
    // ── 1) 예제 conf 로드 ──
    RoomEQ eq;                    // 생성 시 ROOMTUNE_EQ_FILE 자동 로드
    std::printf("== RoomEQ 파서+DSP 재검증 ==\n");
    std::printf("source=%s  bands=%zu  loaded=%d\n",
                eq.source_path().c_str(), eq.band_count(), (int)eq.loaded());

    check("파일 로드됨", eq.loaded());
    // Filter 1~6 ON + Filter 7 OFF → 6밴드
    check("OFF 필터 스킵 (band_count==6)", eq.band_count()==6,
          eq.band_count()==6 ? "" : "(기대 6)");

    eq.set_enabled(true);
    check("enabled 토글", eq.enabled());

    // ── 2) 주파수 응답 (채널 0=L) ──
    std::printf("주파수 응답 (ch0, preamp -4dB 포함):\n");
    double g42   = gain_db(eq, 42.0,   8, 0);   // PK -6dB cut
    double g240  = gain_db(eq, 240.0,  8, 0);   // PK +2.5dB boost
    double g1k   = gain_db(eq, 1000.0, 8, 0);   // 필터 영향 최소 → preamp 지배
    double g16k  = gain_db(eq, 16000.0,8, 0);   // HS -3dB shelf 평탄부
    std::printf("   42Hz=%.2f dB  240Hz=%.2f dB  1kHz=%.2f dB  16kHz=%.2f dB\n",
                g42, g240, g1k, g16k);

    // 42Hz 는 -6dB PK cut 이 지배 → 1kHz(거의 preamp만)보다 확실히 낮아야
    check("42Hz PK cut < 1kHz", g42 < g1k - 2.0);
    // 240Hz 는 +2.5dB boost → preamp(-4) 를 부분 상쇄 → 1kHz 보다 높아야
    check("240Hz PK boost > 1kHz", g240 > g1k + 1.0);
    // 16kHz 는 HS -3dB → 1kHz 보다 낮아야
    check("16kHz HS cut < 1kHz", g16k < g1k - 1.0);
    // 1kHz 는 대략 preamp(-4dB) 근처 (±2dB)
    check("1kHz ≈ preamp -4dB", std::fabs(g1k - (-4.0)) < 2.0,
          g1k > -6 && g1k < -2 ? "" : "(범위 밖)");

    // ── 3) HI(6)/VI(7) 채널 제외 ──
    // 8채널 신호에 240Hz(부스트 대역) 통과 후, ch6/ch7 은 불변(=입력 그대로)이어야
    {
        unsigned N=2048, ch=8;
        std::vector<float> buf(N*ch, 0.0f), ref(N*ch, 0.0f);
        for (unsigned n=0;n<N;++n)
            for (unsigned c=0;c<ch;++c) {
                float v=(float)std::sin(2*M_PI*240.0*n/FS);
                buf[(size_t)n*ch+c]=v; ref[(size_t)n*ch+c]=v;
            }
        eq.process(buf.data(), N, ch);
        bool ch6_same=true, ch7_same=true, ch2_changed=false;
        for (unsigned n=0;n<N;++n){
            if (buf[(size_t)n*ch+6]!=ref[(size_t)n*ch+6]) ch6_same=false;
            if (buf[(size_t)n*ch+7]!=ref[(size_t)n*ch+7]) ch7_same=false;
            if (buf[(size_t)n*ch+2]!=ref[(size_t)n*ch+2]) ch2_changed=true;
        }
        check("HI(ch6) 보정 제외 (불변)", ch6_same);
        check("VI(ch7) 보정 제외 (불변)", ch7_same);
        check("일반 채널(ch2) 은 처리됨", ch2_changed);
    }

    // ── 4) +6dB 부스트 클램프 (임시 conf) ──
    {
        const char* tmp = "/tmp/roomtune_clamp_test.conf";
        FILE* f=std::fopen(tmp,"w");
        std::fprintf(f, "Preamp: 0.0 dB\nFilter 1: ON PK Fc 1000.0 Hz Gain 12.0 dB Q 1.000\n");
        std::fclose(f);
        setenv("ROOMTUNE_EQ_FILE", tmp, 1);
        RoomEQ eq2;               // +12dB 요청 → +6dB 로 클램프되어야
        eq2.set_enabled(true);
        double g1k_boost = gain_db(eq2, 1000.0, 2, 0);
        std::printf("클램프 테스트: 1kHz +12dB 요청 → 실측 %.2f dB (기대 ≈ +6dB)\n", g1k_boost);
        check("부스트 +6dB 클램프", g1k_boost > 4.5 && g1k_boost < 7.5,
              g1k_boost >= 7.5 ? "(클램프 안됨!)" : "");
        std::remove(tmp);
    }

    std::printf("\n== 결과: %s (%d fail) ==\n", fails==0?"ALL PASS":"FAIL", fails);
    return fails==0 ? 0 : 1;
}
