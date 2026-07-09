# DCP-o-matic macOS(Apple Silicon) 소스빌드 계획

> **목적:** M4 Mac Studio에서 DCP-o-matic(및 Player)을 소스로 빌드하는 것을 **게이트**로 삼아, 성공 시 재생 경로에 룸튜닝 EQ(B-chain)를 심는 포크 방향을 확정한다.
> **상태:** 계획 수립 완료. **실제 빌드는 사용자 요청 시 함께 진행**(본 문서는 그 준비).
> **작성일:** 2026-07-09 / 대상 리그: M4 Max Mac Studio, macOS 26.5.1, 36GB
> **함께 볼 문서:** [리서치-종합.md](리서치-종합.md)(§DCP-o-matic 내부구조·빌드), [프로젝트-히스토리.md](프로젝트-히스토리.md)

---

## 0. 리서치 요지 (계획의 근거)

| 발견 | 내용 | 출처 |
|------|------|------|
| 공식 빌드는 brew 클린빌드 아님 | Carl의 `osx-environment`(~40개 라이브러리 소스빌드) + `cdist`(패키징)에 의존 | [dcpomatic.com/building/osx](https://dcpomatic.com/building/osx), 로컬 `BUILD.md`·`cscript` |
| arm64 3줄 레시피 존재 | `copy_resources.sh` → `set_paths.sh` → `./waf configure --target-macos-arm64` (단 osx-environment 선빌드 전제) | 로컬 `DEVELOP.md`, `platform/osx/set_paths.sh` |
| **패치 FFmpeg는 선택** | `av_ebur128_get_true_peaks`는 **라우드니스/true-peak 분석 전용**, `mandatory=False`. 없으면 그 기능만 조용히 빠지고 **빌드는 성공** | 로컬 `wscript`, git.carlh.net/git/ffmpeg.git (dcpomatic-16) |
| 자매 라이브러리 핀버전 | libdcp v1.10.58, libsub v1.6.62, leqm-nrt d75d0af, libttf v0.0.5, libcxml v0.17.3(전이), rtaudio f619b76, openssl 54298369(인증서 생성 패치) | 로컬 `cscript` (dependencies()) |
| 공개 성공사례 희소 | 일반 사용자의 Apple Silicon **from-source** 성공기 사실상 없음. 포럼은 Carl의 크로스빌드 바이너리 테스트기 위주 | dcpomatic.com/forum t=1605·1612·1649 |

> **핵심 전략적 함의:** 우리 목표는 **Player 재생 경로에 EQ 삽입**이지, 라우드니스 분석이 아니다. 따라서 **패치 FFmpeg(가장 큰 함정)를 건너뛸 수 있다.** 이것이 난이도를 실질적으로 낮춘다.

---

## 1. 소스빌드 시 필요한 개발환경 구축

> [개발환경-셋업가이드.md](개발환경-셋업가이드.md)의 iOS/macOS 앱 환경과 **별개**로, C++ 소스빌드용 툴체인이 필요하다. 다행히 상당수는 이미 있음.

### 1.1 이미 갖춰진 것 (측정 확인됨)
- macOS 26.5.1, M4 Max, 36GB ✅
- Command Line Tools(clang 21) ✅ / Homebrew 6.0.8 ✅ / git ✅
- Python 3 (waf 구동), Swift/clang 툴체인 ✅

### 1.2 추가로 필요한 것
| 항목 | 조달 | 비고 |
|------|------|------|
| **Xcode.app** | App Store (셋업가이드 Phase A) | CLT만으로도 waf 컴파일은 가능하나, SDK/서명 위해 권장 |
| **CMake** | `brew install cmake` | 일부 의존성(osx-environment 라이브러리) 빌드에 필요 |
| **pkg-config** | `brew install pkg-config` | waf가 의존성 탐색 시 사용 |
| **autoconf/automake/libtool/nasm/yasm** | `brew install autoconf automake libtool nasm yasm` | FFmpeg·일부 라이브러리 빌드용 |
| **작업 디렉터리** | `$HOME/src/` (권장) | `set_paths.sh`가 `$HOME/src/dcpomatic` 및 `$HOME/osx-environment/arm64/11.0` 경로를 가정 |

> ⚠️ `platform/osx/set_paths.sh`는 경로를 **하드코딩 가정**한다(`$HOME/osx-environment/arm64/11.0`, `$HOME/src/dcpomatic`). 디렉터리 배치를 이에 맞추거나 스크립트를 수정해야 PKG_CONFIG_PATH/LDFLAGS가 어긋나지 않는다.

---

## 2. 소스빌드를 위한 필요 컴포넌트 정리

### 2.1 빌드 대상(우리 소스)
- **dcpomatic** 본체: `dcpomatic.com/download`의 소스 `.tar.bz2`(안정판 2.18.44) 또는 GitHub `cth103/dcpomatic` clone.
- (이미 스크래치패드에 셸로우 클론 존재 — 실제 빌드 시 정식 클론으로 교체)

### 2.2 Carl 자매 라이브러리 (소스빌드 필수, brew 미제공)
| 라이브러리 | 핀버전 | 역할 |
|-----------|--------|------|
| **libcxml** | v0.17.3 (libdcp가 전이 조달) | XML 처리 |
| **libdcp** | **v1.10.58** (osx는 `--c++17`) | DCP I/O 핵심 |
| **libsub** | v1.6.62 | 자막 |
| **libttf** | v0.0.5 | 폰트 |
| **leqm-nrt** | commit `d75d0af` | 라우드니스(Leq(m)) — 우리 용도엔 부차적 |
| **rtaudio** | commit `f619b76` | **오디오 I/O (EQ 삽입 지점의 백엔드)** |

### 2.3 서드파티 (osx-environment 또는 Homebrew)
| 항목 | 최소/권장 | 조달 권장 |
|------|-----------|-----------|
| Boost | ≥1.61 (arm64 패치) | osx-environment (버전 민감) |
| wxWidgets | 3.x | osx-environment (arm64 wx-config 강제, brew 혼용 비권장) |
| FFmpeg | 5.1~7.x | **표준/brew 가능(우리는 패치 불필요)** ※§0 참조 |
| glib/glibmm, pango/pangomm, cairo/cairomm | osx-environment | GUI 스택 |
| ICU | >75시 C++17 강제 | osx-environment/brew |
| libsamplerate, libsndfile, libzip, xmlsec, nettle, openssl(lib), dav1d, x264 | — | osx-environment/brew |
| openssl (인증서 생성 **패치 바이너리**) | commit `54298369` | 소스빌드 (KDM/인증서 기능용) |

### 2.4 우리 목표 기준 "생략 가능" 후보 (난이도 절감)
- **패치 FFmpeg** → 생략(라우드니스 분석만 상실). 표준 FFmpeg 사용.
- **leqm-nrt 정밀성** → 부차적.
- **lwext4(디스크 라이터), ffcmp(테스트)** → 생략.
- 초점은 **Player가 실행되고 다채널 재생이 되는 것**.

---

## 3. 빌드환경 검증 (게이트 이전 사전 점검)

빌드에 뛰어들기 전, **저비용 사전 검증**으로 리스크를 줄인다.

1. **배포판 먼저 확인 (기준선 확보)**
   `brew install --cask dcp-o-matic dcp-o-matic-player` → Player 실행 → **테스트 DCP 재생 + 다채널 출력**이 이 리그에서 정상인지 확인. (소스빌드 결과물과 비교할 기준선)
2. **툴체인 점검:** `xcodebuild -version`, `cmake --version`, `pkg-config --version`, `python3 --version`, `git --version`.
3. **소스 취득·구조 확인:** dcpomatic 클론 후 `DEVELOP.md`/`platform/osx/` 확인, `set_paths.sh` 경로 가정 파악.
4. **의존성 인벤토리:** `brew list`로 이미 있는 것 파악, §2 표와 대조해 부족분 목록화.
5. **빌드 전략 선택**(아래 §4).

---

## 4. 빌드 및 테스트

> 두 가지 접근. **B(실용)부터 시도**하고 막히면 **A(공식)로 폴백**하는 것을 권장.

### 접근 A — 공식 경로 재현 (신뢰도 높음, 무거움)
osx-environment + cdist를 그대로 재현. Carl의 셋업과 동일해 성공확률 높지만 40여 라이브러리 소스빌드로 시간·디스크 큼.
```bash
# 1) 지원 라이브러리 트리 빌드 (~40개, 오래 걸림)
git clone https://git.carlh.net/git/osx-environment.git ~/osx-environment
cd ~/osx-environment
#   config.sh 편집: macOS 11.0 SDK 경로, CMake 경로
./download_all && ./rebuild_all      # → ~/osx-environment/arm64/11.0 생성

# 2) 자매 라이브러리 + (선택)패치 FFmpeg 빌드  ※cdist 또는 수동
#    libcxml → libdcp(v1.10.58,--c++17) → libsub → libttf → leqm-nrt → rtaudio

# 3) dcpomatic 빌드 (arm64 네이티브)
cd ~/src/dcpomatic
bash platform/osx/copy_resources.sh
source platform/osx/set_paths.sh
./waf configure --target-macos-arm64      # 필요시 --disable-tests
./waf
```

### 접근 B — 실용 최소 빌드 (우리 목표 특화, 개척적)
**핵심 착안:** 우리는 **네이티브 arm64 M4**에서 빌드하므로, x86→arm64 **크로스빌드 플래그(`--target-macos-arm64`)가 원리상 불필요**할 수 있다. 자매 라이브러리만 소스빌드하고 나머지는 Homebrew로 충당, **패치 FFmpeg·라우드니스 생략**.
```bash
# 1) brew로 조달 (표준 FFmpeg 포함)
brew install cmake pkg-config boost ffmpeg wxwidgets \
  glib glibmm cairomm pangomm icu4c libsamplerate libsndfile libzip \
  xmlsec1 nettle openssl@3 dav1d

# 2) 자매 라이브러리만 소스빌드 (핀버전, --c++17)
#    libcxml v0.17.3 → libdcp v1.10.58 → libsub v1.6.62 → libttf v0.0.5 → rtaudio

# 3) 네이티브 configure (크로스플래그 없이) + Homebrew 경로 지정
cd ~/src/dcpomatic
export PKG_CONFIG_PATH="$(brew --prefix)/lib/pkgconfig:..."
./waf configure --c++17            # --target-macos-arm64 생략 시도
./waf build
```
- **리스크:** waf의 arm64 경로가 osx-environment 전제로 배선돼 있어, brew wxWidgets/Boost와 충돌하거나 configure가 실패할 수 있음(공개 성공사례 없음). 막히면 접근 A.
- `--warnings-are-errors`가 기본이라 최신 clang에서 경고→에러 가능 → 필요시 이 옵션 제거.

### 4.1 빌드 테스트 (성공 판정)
- `build/src/tools/dcpomatic_player`(Player) 실행
- **테스트 DCP 재생 + 다채널(2.0/5.1) 출력 정상** 확인
- §3-1의 배포판 기준선과 동작 비교
- (선택) configure 로그에서 EBUR128 patched FFmpeg = no 확인(예상대로 라우드니스만 비활성)

---

## 5. 빌드가 끝나면 향후 개발방향 결정

빌드 결과에 따라 분기(게이트):

| 결과 | 결정 |
|------|------|
| ✅ **빌드·재생 성공** | **포크 확정.** 재생 콜백(`src/wx/film_viewer.cc`, `get_audio()` 직후)에 채널별 biquad EQ+레벨+딜레이 삽입 → 설정 UI·프리셋(Config) → X-curve 타깃(소형룸 변형). 별도 브랜치에서 개발, 업스트림 추적. **본 레포를 공개 준비**([공개전환-체크리스트.md](공개전환-체크리스트.md)). |
| ⚠️ **빌드 성공, 유지보수 부담 큼** | 포크는 유지하되 최소 패치로 관리, 또는 하드웨어 DSP 병행 검토 |
| ❌ **빌드 실패/과도한 고통** | **하드웨어 DSP로 선회**(miniDSP DDRC-88A/BM 또는 Flex Eight + Dirac). iPhone/REW는 측정 두뇌로 활용. 포크 포기. |

> 어느 경우든 **iPhone 측정 도구(또는 REW+UMIK)** 는 "어떤 EQ가 필요한지 계산하는 두뇌"로 유효하다. 빌드 게이트는 *적용 경로*(소프트웨어 포크 vs 하드웨어 DSP)를 확정하는 결정점이다.

---

## 부록. 실제 빌드 세션 시 준비물 체크
- [ ] 시간 확보(접근 A는 osx-environment 빌드로 상당히 길 수 있음 — 근거 있는 공개 소요시간 데이터 없음)
- [ ] 디스크 여유(수십 GB)
- [ ] 안정적 네트워크(다수 리포 클론)
- [ ] `brew install --cask dcp-o-matic-player`로 기준선 먼저 확보
- [ ] 막히면 dcpomatic.com/forum·GitHub 이슈(arm64/build 태그)에 문의 — Carl이 유일 공식 빌드 유지자

> ⚠️ 주의: 공식 building/osx 및 일부 포럼은 봇 차단(403)이 있어 브라우저 UA로 접근 필요. AUR 의존성 목록은 Arch 기준 참고용(버전 constraint 미확정). FFmpeg base/브랜치 매핑은 dcpomatic 버전별로 유동적.
