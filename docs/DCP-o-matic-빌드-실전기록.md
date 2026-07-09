# DCP-o-matic macOS(Apple Silicon) 소스빌드 — 실전 성공 기록 (재현 레시피)

> **결과: 성공** ✅ — 2026-07-09, **M4 Max Mac Studio / macOS 26.5.1**에서 **DCP-o-matic v2.18.44** 전체(도구 15종)를 **네이티브 arm64**로 소스 빌드. **완전 원격 · 무sudo · GUI 개입 0.**
> 이는 리서치 시점 "일반 사용자의 Apple Silicon from-source 공개 성공사례 없음"이던 영역을 실제로 뚫은 기록이다.
> **전략:** 공식 osx-environment/cdist 대신 **Homebrew 기반 접근 B** + Carl 자매 라이브러리만 소스빌드 + **C++17 경로** + **패치 FFmpeg 생략(라우드니스만 미지원)**.

---

## 결과 요약
- 산출물: `~/src/dcpomatic/build/src/tools/dcpomatic2_player` (Mach-O **arm64**, ~973K) 외 14개 도구
- 링크: **ffmpeg@7**(libavcodec.61=7.1), libdcp-1.0, libcxml, libasdcp-dcpomatic 등
- 미지원(의도적): EBUR128 true-peak/라우드니스 분석(패치 FFmpeg 미사용) — 룸튜닝 용도엔 무관
- 환경 스크립트: [`scripts/roomtune-env.sh`](../scripts/roomtune-env.sh)

## 0) 사전 상태 (이 리그)
- macOS 26.5.1, M4 Max, 36GB / Command Line Tools(clang 21, SDK 26.5) / Homebrew / git — 사전 구비
- **Xcode.app 불필요** (CLT만으로 waf C++ 빌드 성공)

## 1) 빌드 툴 & 의존성 (Homebrew, 무sudo)
```bash
brew install pkg-config cmake nasm
brew install ffmpeg@7 wxwidgets boost xerces-c libxml++@4 libsamplerate libsndfile \
             libzip icu4c glibmm cairomm pangomm libssh xmlsec1 openjpeg fmt fast_float \
             rtaudio libharu
```
- **ffmpeg@7** (brew 기본 ffmpeg는 8.x → API 불일치, @7로 고정)
- **libxml++@4** (C++17 경로가 요구하는 libxml++-4.0 제공; 기본 `libxml++`는 2.6)
- glibmm/cairomm/pangomm 최신(→ glibmm-2.68/cairomm-1.16/pangomm-2.48 = C++17 바인딩 세트와 일치)
- libharu (DCP 검증 PDF 리포트용 — 없으면 `dcp/pdf_formatter.h` 누락 에러)

## 2) 환경 설정
`source scripts/roomtune-env.sh` — PREFIX=`$HOME/dcpomatic-env`, ffmpeg@7 우선순위, keg-only(icu4c@78·openssl@3) 경로 포함.

## 3) 소스 취득 (버전 고정)
```bash
# 본체 (버전감지 위해 태그 고정; shallow OK)
git clone --depth 1 --branch v2.18.44 https://github.com/cth103/dcpomatic.git ~/src/dcpomatic
# 자매 라이브러리는 GitHub가 아니라 git.carlh.net 에 있음
git clone --depth 1 --branch v0.17.17 https://git.carlh.net/git/libcxml.git   ~/src/libcxml
git clone --depth 1 --branch v1.0.9    https://git.carlh.net/git/asdcplib.git  ~/src/asdcplib
git clone --depth 1 --branch v1.10.58  https://git.carlh.net/git/libdcp.git    ~/src/libdcp
git clone --depth 1 --branch v1.6.62   https://git.carlh.net/git/libsub.git    ~/src/libsub
git clone --depth 1 --branch v0.0.5    https://git.carlh.net/git/libttf.git    ~/src/libttf
git clone https://git.carlh.net/git/leqm-nrt.git ~/src/leqm-nrt && git -C ~/src/leqm-nrt checkout d75d0af
```

## 4) 자매 라이브러리 빌드 (순서 중요, `source scripts/roomtune-env.sh` 후)
> Carl 라이브러리는 모두 waf. libcxml/libdcp는 **`--c++17`** 필수(그래야 libxml++-4.0 사용, DCP-o-matic C++17과 정합).
```bash
# libcxml (c++17 → libxml++-4.0)
( cd ~/src/libcxml && python3 ./waf configure --prefix=$PREFIX --c++17 && python3 ./waf build && python3 ./waf install )
# asdcplib (libasdcp-dcpomatic 제공)
( cd ~/src/asdcplib && python3 ./waf configure --prefix=$PREFIX && python3 ./waf build && python3 ./waf install )
# libdcp (c++17 + 예제/PDF덤프/테스트 비활성; libharu는 자동감지되어 pdf_formatter.h 생성)
( cd ~/src/libdcp && python3 ./waf configure --prefix=$PREFIX --c++17 --disable-tests --disable-examples --disable-dumpimage && python3 ./waf build && python3 ./waf install )
# libsub (c++17 옵션 없음 — 기본; 단 icu 경로가 env에 있어야 boost::locale 통과)
( cd ~/src/libsub && python3 ./waf configure --prefix=$PREFIX --disable-tests && python3 ./waf build && python3 ./waf install )
# libttf (--disable-tests 필요: boost unit_test 체크 회피)
( cd ~/src/libttf && python3 ./waf configure --prefix=$PREFIX --disable-tests && python3 ./waf build && python3 ./waf install )
# leqm-nrt (mandatory) — ★ src/wscript 패치 필요 (아래 함정 4)
( cd ~/src/leqm-nrt && python3 ./waf configure --prefix=$PREFIX && python3 ./waf build && python3 ./waf install )
```

## 5) DCP-o-matic 본체
```bash
source scripts/roomtune-env.sh
cd ~/src/dcpomatic
python3 ./waf configure --prefix=$PREFIX \
        --wx-config=$(brew --prefix wxwidgets)/bin/wx-config \
        --c++17 --disable-tests
python3 ./waf build          # → build/src/tools/dcpomatic2_player 등
```
- **`--c++17`**: cpp17 + libxml++-4.0 + pangomm-2.48/cairomm-1.16/glibmm-2.68 (brew와 정합)
- **`--wx-config=.../wxwidgets/bin/wx-config`**: arm64 경로가 osx-environment wx-config를 강제하므로 brew wx로 대체
- ❌ **`--target-macos-arm64` 쓰지 말 것**: 그건 x86→arm64 크로스빌드(osx-environment 전제)용. 네이티브 arm64에선 불필요.

---

## 겪은 함정과 해결 (macOS 특유 — 재현 시 그대로 발생)
1. **자매 라이브러리 태그가 GitHub에 없음** → `git.carlh.net`에서 클론.
2. **libxml++ 세대 불일치**: 기본 `libxml++`(2.6)의 `std::auto_ptr`가 C++17에서 제거됨 → 54개 에러. 해결: `--c++17` + `libxml++@4`(libxml++-4.0), 그리고 libcxml/libdcp도 `--c++17`로 빌드.
3. **libdcp .pc가 stale**: 비-c++17로 먼저 빌드 후 `--c++17` 재configure해도 `.pc`가 `libxml++-2.6`을 유지 → **`./waf distclean` 후 재빌드**해야 `.pc`가 libxml++-4.0으로 갱신됨.
4. **leqm-nrt 공유 라이브러리 sndfile 링크 누락(버그)**: macOS는 dylib 미정의 심볼 불허 → `sf_open` 등 링크 실패. 해결: `src/wscript`에서 라이브러리 타깃에 `obj.uselib = 'SNDFILE'` 추가:
   ```python
   obj.source = 'leqm_nrt.cc'
   if bld.env.WITH_LIBSNDFILE:
       obj.uselib = 'SNDFILE'
   ```
5. **ffmpeg 8 vs @7 충돌**: brew 기본 ffmpeg(8)의 libav*.pc가 먼저 잡힘 → env에서 `ffmpeg@7`를 PKG_CONFIG_PATH 앞에 둬 우선.
6. **ICU keg-only 경로 누락**: libsub의 boost::locale 체크가 `-licuuc`를 못 찾음 → LDFLAGS/CPPFLAGS에 `icu4c@78` 경로 명시.
7. **libharu 미설치 → `dcp/pdf_formatter.h` 없음**: `brew install libharu` 후 libdcp 재빌드.
8. **libttf/leqm-nrt 옵션 차이**: libttf는 `--disable-tests` 있음(써야 함), leqm-nrt는 없음(빼야 함).

## 검증(원격 가능 범위)
- `file dcpomatic2_player` → `Mach-O 64-bit executable arm64` ✅
- `otool -L` → ffmpeg@7 · libdcp-1.0 링크 확인 ✅
- ⚠️ **GUI 실행 / DCP 재생 / EQ 청취**는 **스피커 앞 물리적 위치 + 화면공유**가 필요(원격 목표 아님).

## 다음 단계 (P1 확정)
빌드 게이트 통과 → **포크 확정**. 재생 콜백 `src/wx/film_viewer.cc`의 `_butler->get_audio(...)`(약 742행) 직후에 채널별 biquad EQ+레벨+딜레이 삽입 → 설정 UI/프리셋 → X-curve(소형룸) 타깃. 상세: [DCP-o-matic-빌드계획.md](DCP-o-matic-빌드계획.md) §5, [개발명세서.md](개발명세서.md).
