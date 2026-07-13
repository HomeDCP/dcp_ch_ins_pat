# RoomTune 재생 EQ + DCP 검수용 채널 인스펙터 (통합 패치)

DCP-o-matic Player v2.18.44 재생 경로에 두 기능을 더한다:

1. **재생 룸튜닝 EQ**(`View > Room EQ (test)`) — REW/EQ-APO 측정 파일 기반 채널별 biquad EQ.
   내보내는 DCP엔 영향 없음(재생 전용). 상세: `docs/측정-룸튜닝-워크플로.md`.
2. **채널 인스펙터**(`Tools > Channel inspector...`) — DCP 채널 매핑을 실측 검증하는 검수 도구.
   채널별 **solo/mute** + **실시간 피크 미터**(dBFS).

> 이 패치는 `patches/roomtune-playback-eq/`(EQ 단독, self-contained `room_eq.h`)의 **상위집합**이다.
> 여기서는 biquad DSP 코어를 `biquad.h`로 분리해 EQ와 인스펙터가 공유한다. 둘 중 하나만 적용할 것
> — `room_eq.h` 정의가 다르므로 두 패치를 함께 적용하지 말 것.

## 구성 파일 (베이스 v2.18.44)

| 파일 | 역할 |
|---|---|
| `src/wx/biquad.h` (신규) | 공유 RBJ biquad 코어(`roomtune::` 네임스페이스). EQ·인스펙터 공용. |
| `src/wx/room_eq.h` (신규, biquad.h 사용) | 재생 EQ 파서+DSP. 측정 파일 로더. |
| `src/wx/channel_inspector.h` (신규) | 채널 인스펙터 실시간 엔진(wx 비의존). solo/mute·피크·무할당 다운믹스 매트릭스(SPSC 더블버퍼). |
| `src/wx/channel_inspector_dialog.h` (신규) | 비모달 UI(채널행 solo/mute 체크박스 + 피크 미터, 헤더온리 **wxFrame**). |
| `src/wx/film_viewer.{h,cc}` (수정) | 지연 identity Butler 배선 + audio_callback 분기 + 포워더. include guard 추가. |
| `src/tools/dcpomatic_player.cc` (수정) | `Tools > Channel inspector...` 메뉴/Bind/다이얼로그. |

## 핵심 아키텍처 — 지연 identity화 (회귀 0)

- 채널 인스펙터를 **처음 열 때만** Butler를 identity 매핑 + DCP 채널수로 재구성한다. 그 전까지
  기존 재생 경로(`Config::audio_mapping`)는 **무변경** → 인스펙터를 안 쓰면 재생 품질에 영향이 없다.
- 활성 시: Butler가 DCP 레이아웃(N ch)으로 오디오를 내주고, RtAudio 콜백이 **무할당** 다운믹스
  매트릭스로 격리·라우팅한다(스테레오·헤드폰·다채널 모두 커버). solo/mute는 lock-free 발행으로
  실시간 반영. solo 없음 = 현행 출력 매핑 그대로(mute 채널만 제거)라 출력이 종전과 동일.
- **solo = solo-in-place**: solo된 채널을 원래 출력 라우팅(Config 매핑) 그대로 두고 나머지 채널을 죽인다.
  L solo=왼쪽만(원래 음량 유지), C solo=원래대로 양쪽. 모노합으로 2채널 강제 스플릿하지 않는다. HI(6)/VI(7) 포함 전 채널 노출.

## 적용법

```bash
cd ~/src/dcpomatic          # stock v2.18.44 소스 트리
git apply patches/.../0001-roomtune-playback-eq-and-channel-inspector.patch
# 또는 신규 헤더는 사본을 직접 복사(biquad.h/channel_inspector*.h/room_eq.h → src/wx/)
```

빌드(환경은 `scripts/roomtune-env.sh`):
```bash
source scripts/roomtune-env.sh
cd ~/src/dcpomatic
python3 waf build      # ./waf 는 shebang이 python 을 찾으므로 python3 명시
```
증분 빌드 성공 검증됨(2026-07-12, `[518/518] dcpomatic2_player`, 10s).

## 검증 상태 (2026-07-13)

- **헤드리스 단위검증 PASS:**
  - biquad.h 분리 후 룸EQ 응답 무변화(회귀 하니스 ALL PASS).
  - 채널 인스펙터 엔진 16/16 PASS — downmix 정확성, 피크미터, solo/mute, lock-free publish 스위칭, 채널 클램프, 비활성 폴백.
- **실기 GUI 검증 완료(2026-07-13):** `Tools > Channel inspector...` — solo/mute 실시간 반영·채널 격리 확인.
- **실기 중 발견·수정한 이슈 2건:**
  1. **창이 저절로 닫힘** — macOS `wxDialog`가 자식 체크박스의 command 이벤트를 취소(close)로 오인 →
     `wxEVT_CLOSE_WINDOW`에서 인스펙터가 비활성화되어 solo/mute가 즉시 풀림. **`wxFrame`으로 전환**해 해결.
  2. **solo가 양쪽으로 스플릿** — 초기 solo가 모노합(×0.707)이라 격리 채널이 양쪽 스피커로 나뉘어 음량↓·직관 위배.
     **solo-in-place**(원래 라우팅 유지)로 변경해 해결.

## v1 범위 / 다음

- **v1(이 패치):** 채널 검수 코어 — solo/mute + 피크미터.
- **다음 반복:** 진단 EQ 프리셋(Dialogue/LFE/Surround/HF) + 전역 X-curve/Flat. 설계 완본:
  `docs/채널-인스펙터-설계.md` §5·Phase 5. biquad.h 코어는 이미 공유 준비됨.
