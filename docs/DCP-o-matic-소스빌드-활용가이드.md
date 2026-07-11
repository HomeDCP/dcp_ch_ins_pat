# DCP-o-matic 소스빌드 & 활용 가이드

> M4 Mac Studio에서 DCP-o-matic을 **소스로 빌드**한 전 과정과, 그 **정식(stock) 빌드를 다른 프로젝트에서 가져다 쓰는 법**을 한 문서로 정리한다.
> **핵심:** 정식 빌드는 `~/src/dcpomatic-stock/build/src/tools/`에 있고, **제자리에서 절대경로로 호출**하면 13종 전체 도구를 그대로 쓸 수 있다.
> 작성일 2026-07-11 · 대상 DCP-o-matic v2.18.44 (arm64 네이티브)

---

## 0. 이 문서는
- **누가:** 이 맥에서 DCP-o-matic 도구를 쓰려는 다른 프로젝트(예: GPU J2K 인코딩)
- **무엇을:** 소스빌드 여정 + 결과물 위치 + **모든 도구 활용법**
- **왜 소스빌드:** 향후 인코딩/재생 경로를 직접 개조(GPU 연산, 룸 EQ 등)하기 위해 — 배포판(.app)으론 불가

---

## 1. 우리가 한 일 (여정 한눈에)
1. **룸튜닝 솔루션** 기획 → 음향학·측정·macOS 오디오 딥리서치.
2. 방향 전환: 시스템 전역 EQ 대신 **DCP-o-matic 재생 출력만 정밀 튜닝**(소프트웨어 B-chain)로 좁힘.
3. **빌드 게이트:** "macOS에서 DCP-o-matic 소스빌드가 되는가?" → **성공**(공개 사례 없던 arm64 from-source를 뚫음). 접근 B = Homebrew + Carl 자매 라이브러리 소스빌드 + C++17 + 패치 FFmpeg 생략.
4. **EQ 삽입 PoC:** 재생 콜백(`film_viewer.cc`)에 채널별 biquad EQ 주입 → 소리 차이 확인(성공).
5. **정식(stock) 빌드 분리:** EQ 없는 순정 v2.18.44 전체 도구를 별도로 빌드 → **GPU J2K 인코딩 프로젝트의 베이스/검증 도구**로 재사용.

> 상세 여정: [프로젝트-히스토리.md](프로젝트-히스토리.md) · 리서치: [리서치-종합.md](리서치-종합.md)

---

## 2. 무엇이 만들어졌나 (결과물)
| 빌드 | 위치 | 성격 |
|------|------|------|
| **정식(stock)** | `~/src/dcpomatic-stock/build/src/tools/` (`$DCPOMATIC_TOOLS`) | **EQ 없음. 다른 프로젝트가 쓸 것.** |
| 룸튜닝(EQ) | `~/src/dcpomatic/build/src/tools/` (`$ROOMTUNE_TOOLS`) | 재생 EQ 변형(별개) |
| 자매 라이브러리 | `~/dcpomatic-env/` | 두 빌드가 공유(libdcp·libcxml·libsub 등) |

둘 다 **arm64 네이티브**, v2.18.44. 아래 활용법은 **정식 빌드 기준**.

---

## 3. ★ 정식(stock) 빌드 활용법 — 핵심

### 3.1 준비 (한 번)
```bash
# 편의 환경 소싱 (경로/헬퍼 정의)
source /Users/homedcp/Claude/Projects/roomtunning/scripts/roomtune-tools.env.sh
# → DCPOMATIC_TOOLS=~/src/dcpomatic-stock/build/src/tools  (정식)
#   dcpm-player / dcpm-cli / dcpm-verify / dcpm-create / dcpm-server 헬퍼 정의됨
```
소싱이 싫으면 그냥 절대경로로 호출해도 동일하게 동작한다(dylib가 절대경로로 링크됨 → env 불필요).

### 3.2 전체 도구 레퍼런스 (13종)
> 경로 = `$DCPOMATIC_TOOLS/<도구>`. GUI 도구는 실행하면 창이 뜨고, CLI 도구는 터미널에서 동작.

#### 재생·검증 (GPU 프로젝트에서 자주 씀)
| 도구 | 유형 | 용도 / 예시 |
|------|------|-------------|
| **`dcpomatic2_player`** | GUI | DCP·중간산출물 재생/검사. `dcpm-player "/DCP폴더"` |
| **`dcpomatic2_verifier`** | GUI+CLI | DCP 규격 검증. `dcpm-verify -s "/DCP폴더"` (또는 창 실행) |

#### 인코딩 (GPU 연산을 붙일 대상)
| 도구 | 유형 | 용도 / 예시 |
|------|------|-------------|
| **`dcpomatic2_create`** | CLI | 콘텐츠(영상)→필름 디렉토리 생성. `dcpm-create -n "MyFilm" -c FTR -o ~/films/my /경로/video.mov` |
| **`dcpomatic2_cli`** | CLI | **필름→DCP 인코딩(J2K 인코딩 수행).** `dcpm-cli make-dcp ~/films/my` · 설정덤프 `dcpm-cli dump ~/films/my` · 서버목록 `dcpm-cli list-servers` |
| **`dcpomatic2_server_cli`** | CLI | **분산 인코딩 워커(헤드리스).** `$DCPOMATIC_TOOLS/dcpomatic2_server_cli -t 8 --verbose` |
| `dcpomatic2_server` | GUI | 분산 인코딩 서버(창 버전) |
| `dcpomatic2` | GUI | 메인 앱 — 필름 생성·설정·인코딩을 UI로 |
| `dcpomatic2_batch` | GUI | 여러 필름 배치 인코딩 대기열 |

#### DCP 가공·관리
| 도구 | 유형 | 용도 / 예시 |
|------|------|-------------|
| `dcpomatic2_combiner` | CLI/GUI | 여러 DCP(CPL)를 하나로 결합. `dcpomatic2_combiner --verbose ...` |
| `dcpomatic2_map` | CLI | DCP 리맵(오디오 채널 재배치 등). *(현재 `--help` 출력에 버그 있음 — 인자 형식은 소스/문서 확인)* |
| `dcpomatic2_editor` | GUI | DCP 메타데이터 편집 |
| `dcpomatic2_playlist` | GUI | 상영 재생목록(SPL) 편집 |

#### 보안(KDM)
| 도구 | 유형 | 용도 / 예시 |
|------|------|-------------|
| `dcpomatic2_kdm_cli` | CLI | KDM 생성/관리. `create` · `list-cinemas` · `add-dkdm` · `dump-decryption-certificate` |
| `dcpomatic2_kdm` | GUI | KDM 생성(창 버전) |

### 3.3 실전 워크플로 예시 — GPU J2K 인코딩 프로젝트
```bash
source /Users/homedcp/Claude/Projects/roomtunning/scripts/roomtune-tools.env.sh

# 1) 소스 영상 → 필름 디렉토리 생성
dcpm-create -n "GPUTest" -c TST -o ~/films/gputest "/경로/source.mov"

# 2) 필름 → DCP 인코딩  ← 이 단계의 J2K 인코딩에 GPU 연산을 붙여 테스트
dcpm-cli make-dcp ~/films/gputest        # 결과 DCP는 필름 폴더 안에 생성

# 3) 결과 DCP 규격 검증
dcpm-verify -s ~/films/gputest/<DCP폴더명>

# 4) 중간산출물/DCP 재생 확인
dcpm-player ~/films/gputest/<DCP폴더명>
```
- **GPU 연산 삽입 지점:** J2K 인코딩은 `dcpomatic2_cli make-dcp`(및 분산 서버)에서 수행되며, 실제 압축은 `libdcp`의 `dcp::compress_j2k` / dcpomatic의 `J2KEncoder` 계열이 담당한다. GPU를 붙이려면 이 지점을 개조해야 하므로, **정식 소스(`~/src/dcpomatic-stock`, `~/src/libdcp`)를 베이스로 별도 포크**를 권장한다. (정확한 삽입 지점 탐색은 별도 지원 가능.)

### 3.4 반드시 지킬 규칙 ⚠️
- ✅ **제자리에서 실행** (`~/src/dcpomatic-stock/build/src/tools/`).
- ❌ **바이너리만 딴 곳으로 복사 금지** — dylib(절대경로)·리소스를 못 찾아 실패.
- ❌ **build 디렉토리 이동/`./waf distclean` 금지** — 산출물·리소스 소멸.
- 리소스는 `~/src/dcpomatic-stock/build/src/Resources/`에 있어야 함(실행파일 기준 `../Resources` 자동 탐색 → **cwd 무관**).
- brew 패키지(ffmpeg@7 등)·`~/dcpomatic-env`·`~/src/*`를 삭제/이동하지 말 것(절대경로 링크 대상).

---

## 4. 소스 빌드 과정 (요약)
> 정식 빌드를 다시 만들거나 다른 머신에 재현할 때. 전체 레시피·함정은 [DCP-o-matic-빌드-실전기록.md](DCP-o-matic-빌드-실전기록.md) 참조.

1. **빌드툴/의존성(Homebrew, 무sudo):** `pkg-config cmake nasm ffmpeg@7 wxwidgets boost xerces-c libxml++@4 libsamplerate libsndfile libzip icu4c glibmm cairomm pangomm libssh xmlsec1 openjpeg fmt fast_float rtaudio libharu`
2. **환경:** `source scripts/roomtune-env.sh` (PREFIX=`~/dcpomatic-env`, ffmpeg@7 우선, C++17 경로)
3. **자매 라이브러리(waf, 순서대로):** libcxml `--c++17` → asdcplib → libdcp `--c++17 --disable-tests --disable-examples --disable-dumpimage` → libsub → libttf `--disable-tests` → leqm-nrt(★ src/wscript에 sndfile uselib 패치) — 소스는 `git.carlh.net`
4. **본체:** `./waf configure --prefix=$PREFIX --wx-config=$(brew --prefix wxwidgets)/bin/wx-config --c++17 --disable-tests && ./waf build`
5. **리소스 스테이징:** `bash platform/osx/copy_resources.sh` + `graphics/*.png`·`osx/*`·폰트를 `build/src/Resources/`로 복사(없으면 splash.png 못 찾아 실행 시 에러)
6. 핵심 함정: libxml++ 2.6→4.0(C++17), ffmpeg@7 고정(8 아님), libdcp distclean 재빌드, libharu 필요, `--target-macos-arm64` 쓰지 말 것.

---

## 5. 룸튜닝(EQ) 빌드 (참고, 별개)
`~/src/dcpomatic`는 재생 콜백(`film_viewer.cc`의 `get_audio()` 직후)에 채널별 biquad EQ(`room_eq.h`)를 삽입한 변형이다. View 메뉴 "Room EQ" 토글. **재생 전용** — 내보내는 DCP엔 영향 없음. GPU 프로젝트와는 무관하니 정식 빌드를 쓰면 된다.

---

## 6. 트러블슈팅 · 이식성 · 라이선스
- **실행 시 splash.png 에러:** 리소스 미스테이징 → §4-5 재수행.
- **config 크래시(DKDM 저장 시):** EQ 빌드 한정 이슈(libxml++ 4.4.1). 정식 빌드는 무관.
- **CLI가 `fatal:`(git) 경고:** cwd에 따른 무해한 경고, 기능 정상.
- **다른 머신에서 쓰려면:** "같은 맥 참조"는 이 맥 한정. 재빌드(레시피) 또는 `dylibbundler`로 자립 번들 필요.
- **라이선스:** DCP-o-matic 파생물 = **GPL-2.0-or-later**. 내부 사용 무제한, **외부 배포 시 대응 소스 제공 의무**. [공개전환-체크리스트.md](공개전환-체크리스트.md) §1.

---

### 관련 문서
- [도구-재사용-안내.md](도구-재사용-안내.md) — 재사용 간단 버전
- [DCP-o-matic-빌드-실전기록.md](DCP-o-matic-빌드-실전기록.md) — 전체 빌드 레시피·함정
- [DCP-o-matic-빌드계획.md](DCP-o-matic-빌드계획.md) · [프로젝트-히스토리.md](프로젝트-히스토리.md) · [리서치-종합.md](리서치-종합.md)
- 스크립트: [scripts/roomtune-tools.env.sh](../scripts/roomtune-tools.env.sh) · [scripts/roomtune-env.sh](../scripts/roomtune-env.sh)
