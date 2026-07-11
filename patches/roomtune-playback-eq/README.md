# RoomTune 재생 경로 룸 EQ 패치

DCP-o-matic **재생 전용**(FilmViewer 오디오 콜백) 채널별 파라메트릭 EQ.
이 처리는 플레이어 재생 출력에만 적용되며 **내보내는 DCP 에는 영향이 없다**.

- **베이스:** DCP-o-matic `v2.18.44`
- **파생 라이선스:** GPL-2.0-or-later (원작과 동일)
- **삽입 지점:** `src/wx/film_viewer.cc` 의 `audio_callback()` — `get_audio()` 직후,
  인터리브 float32 버퍼에 in-place 처리. 인코딩 경로(`player.cc` AudioProcessor)는 건드리지 않음.

## 구성 파일

| 파일 | 내용 |
|---|---|
| `0001-roomtune-playback-eq.patch` | 완전 통합 패치(신규 `room_eq.h` + 3파일 수정) |
| `room_eq.h` | 가독성용 단독 사본(= 패치 안의 신규 파일과 동일) |

수정 파일:
- `src/wx/room_eq.h` *(신규)* — RBJ biquad 캐스케이드 + REW 파라메트릭 파일 로더
- `src/wx/film_viewer.h` — `RoomEQ _room_eq;` 멤버 + `set_room_eq_enabled()` 토글
- `src/wx/film_viewer.cc` — 콜백에서 `_room_eq.process(...)` 호출
- `src/tools/dcpomatic_player.cc` — `View > Room EQ (test)` 체크 메뉴

## 필터는 어디서 오나 (측정 기반)

하드코딩 데모(구버전 2 kHz high-shelf)는 제거됐다. 이제 필터는 **실측 결과 파일**에서 온다:

1. REW 등으로 스피커+룸을 실측하고 타깃 커브에 맞춘 보정 필터를 만든다.
2. REW **"Filter Settings as text"**(= EQ APO) 포맷으로 저장한다.
3. 다음 경로 중 하나에 둔다:
   - `ROOMTUNE_EQ_FILE` 환경변수가 가리키는 파일, 또는
   - `~/.config/roomtune/room_eq.conf`
4. 플레이어 실행 → `View > Room EQ (test)` 토글.

파일이 없거나 유효 밴드가 0개면 EQ 는 **무처리(no-op)** — 실측 없이는 보정하지 않는다.
자세한 절차는 [`docs/측정-룸튜닝-워크플로.md`](../../docs/측정-룸튜닝-워크플로.md), 예시는
[`examples/room_eq.example.conf`](../../examples/room_eq.example.conf) 참조.

지원 타입: `PK`(peaking), `LS`/`HS`(low/high shelf), `LP`/`HP`(low/high pass) — 모두 Q 기반.
엔진 규칙: 부스트 상한 **+6 dB**(초과 시 클램프), 컷 무제한, `Preamp` 지원(감쇠만),
접근성 채널(HI=6, VI=7) 제외, fs = 48 kHz(DCP 표준) 가정.

## 깨끗한 소스에 적용하기

```bash
cd ~/src/dcpomatic                       # v2.18.44 체크아웃
git apply patches/.../0001-roomtune-playback-eq.patch
# 재빌드: source scripts/roomtune-env.sh && python3 ./waf build
```

## 검증 상태 (2026-07-12)

`room_eq.h` 파서+DSP 를 독립 하니스로 검증(48 kHz 사인 스윕으로 실측 응답 확인):
- PK 중심주파수에서 지정 dB 정확히 일치, LS/HS 평탄부 일치
- 필터 사이 구간은 Preamp 만 반영
- +12 dB 요청 → +6 dB 로 클램프됨(로그 경고)
- OFF/None 필터 스킵, 빈 파일 → no-op
- HI(6)·VI(7) 채널 통과(보정 제외), 비활성 시 무처리
- 전체 dcpomatic 증분 빌드 성공, 플레이어 바이너리에 코드 반영 확인
