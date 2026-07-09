#!/bin/bash
# RoomTune / DCP-o-matic 소스빌드 공용 환경 (각 빌드 전 `source` 할 것)
# 전략: C++17 경로 (libxml++-4.0, glibmm-2.68, cairomm-1.16, pangomm-2.48)
# 검증: 2026-07-09, M4 Max Mac Studio / macOS 26.5.1 / DCP-o-matic v2.18.44 빌드 성공
export PREFIX="$HOME/dcpomatic-env"
BREW="$(brew --prefix)"
mkdir -p "$PREFIX/lib/pkgconfig"

# PKG_CONFIG_PATH: PREFIX + 버전결정적 keg 우선 + 나머지 keg + 표준
# (pkg-config는 첫 매치 사용 → ffmpeg@7이 brew ffmpeg8을 이기게 함)
PC="$PREFIX/lib/pkgconfig"
for k in ffmpeg@7 icu4c@78 openssl@3 libxml++@4; do
  d="$BREW/opt/$k/lib/pkgconfig"; [ -d "$d" ] && PC="$PC:$d"
done
for d in "$BREW"/opt/*/lib/pkgconfig "$BREW"/lib/pkgconfig "$BREW"/share/pkgconfig; do
  [ -d "$d" ] && PC="$PC:$d"
done
export PKG_CONFIG_PATH="$PC"

# keg-only(icu4c@78, openssl@3) 경로를 CPPFLAGS/LDFLAGS에 명시 (boost::locale 등 하드코딩 링크 대응)
export CPPFLAGS="-I$PREFIX/include -I$BREW/include -I$BREW/opt/icu4c@78/include -I$BREW/opt/openssl@3/include"
export LDFLAGS="-L$PREFIX/lib -L$BREW/lib -L$BREW/opt/icu4c@78/lib -L$BREW/opt/openssl@3/lib"
export PATH="$PREFIX/bin:$BREW/opt/ffmpeg@7/bin:$BREW/bin:$PATH"
export DOM_BREW="$BREW"
echo "[env] PREFIX=$PREFIX  BREW=$BREW  (C++17 경로)"
