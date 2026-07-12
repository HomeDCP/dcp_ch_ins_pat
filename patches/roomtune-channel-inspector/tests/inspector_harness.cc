// ChannelInspector 엔진 단위검증 하니스 (헤드리스, wx 비의존)
#include "channel_inspector.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <cstring>
#include <algorithm>

static int fails = 0;
static void check(const char* n, bool ok, const char* d="") {
    std::printf("  [%s] %s %s\n", ok?"PASS":"FAIL", n, d); if(!ok) ++fails;
}

int main() {
    ChannelInspector ci;
    std::printf("== ChannelInspector 엔진 단위검증 ==\n");
    ci.configure(6, 2, 512);   // 5.1 DCP → 스테레오 장치
    check("active_pipeline", ci.active_pipeline());
    check("dcp_channels==6", ci.dcp_channels()==6);
    check("dev_channels==2", ci.dev_channels()==2);

    const unsigned frames=8; const int dcp=6, dev=2;

    // ── 1) downmix 정확성: L→L, R→R, C→(L,R)×0.7, LFE 매핑0(무기여) ──
    ChannelInspector::MatrixSrc M; std::memset(&M, 0, sizeof(M));
    M.g[0][0]=1.0f; M.g[1][1]=1.0f; M.g[2][0]=0.7f; M.g[2][1]=0.7f;
    ci.publish(M);

    std::vector<float> mid(frames*dcp), out(frames*dev, -99.0f);
    for(unsigned f=0;f<frames;++f){
        mid[f*dcp+0]=1.0f+f; mid[f*dcp+1]=2.0f+f; mid[f*dcp+2]=0.5f;
        mid[f*dcp+3]=9.0f;   // LFE — 매핑 0 이므로 출력에 안 나와야
        mid[f*dcp+4]=0.0f; mid[f*dcp+5]=0.0f;
    }
    ci.downmix(mid.data(), out.data(), frames);
    bool ok_dm=true;
    for(unsigned f=0;f<frames;++f){
        float expL=(1.0f+f)+0.5f*0.7f, expR=(2.0f+f)+0.5f*0.7f;
        if(std::fabs(out[f*dev+0]-expL)>1e-5f) ok_dm=false;
        if(std::fabs(out[f*dev+1]-expR)>1e-5f) ok_dm=false;
    }
    check("downmix 정확 (L/R + C×0.7, LFE 무기여)", ok_dm);

    // ── 2) meter: 채널별 피크(원신호 기준) ──
    std::vector<float> m2(frames*dcp, 0.0f);
    for(unsigned f=0;f<frames;++f){ m2[f*dcp+0]=0.5f; m2[f*dcp+1]=1.0f; m2[f*dcp+2]=0.25f; }
    ci.meter(m2.data(), frames);
    check("peak ch0 ≈ -6dB", std::fabs(ci.peak_dbfs(0)-(-6.02f))<0.5f);
    check("peak ch1 ≈ 0dB",  std::fabs(ci.peak_dbfs(1)-0.0f)<0.1f);
    check("peak ch3 무신호 → -120", ci.peak_dbfs(3)<-100.0f);

    // ── 3) solo/mute API ──
    check("초기 has_solo false", !ci.has_solo());
    ci.set_solo(2,true);
    check("solo(2) + has_solo", ci.solo(2) && ci.has_solo());
    ci.set_mute(4,true);
    check("mute(4)", ci.mute(4));
    ci.clear();
    check("clear 후 전부 해제", !ci.has_solo() && !ci.mute(4) && !ci.solo(2));

    // ── 4) publish 스위칭: 최신 매트릭스 반영 (SPSC 더블버퍼) ──
    ChannelInspector::MatrixSrc M2; std::memset(&M2,0,sizeof(M2));
    M2.g[1][0]=1.0f;   // R→L only
    ci.publish(M2);
    std::fill(out.begin(), out.end(), -99.0f);
    ci.downmix(mid.data(), out.data(), frames);
    bool ok_sw=true;
    for(unsigned f=0;f<frames;++f){
        if(std::fabs(out[f*dev+0]-(2.0f+f))>1e-5f) ok_sw=false;  // R→L
        if(std::fabs(out[f*dev+1]-0.0f)>1e-5f)     ok_sw=false;  // dev1 무음
    }
    check("publish 스위칭 반영", ok_sw);

    // ── 5) 채널수 클램프 / 비활성 ──
    ChannelInspector ci2;
    check("configure 전 비활성", !ci2.active_pipeline());
    ci2.configure(99, 99, 256);  // MAX 초과 → 16 클램프
    check("dcp 클램프 16", ci2.dcp_channels()==16);
    check("dev 클램프 16", ci2.dev_channels()==16);
    ci2.configure(0, 2, 256);    // 오디오 없음
    check("dcp 0 → 비활성", !ci2.active_pipeline());

    std::printf("\n== 결과: %s (%d fail) ==\n", fails==0?"ALL PASS":"FAIL", fails);
    return fails==0?0:1;
}
