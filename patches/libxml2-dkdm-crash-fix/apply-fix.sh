#!/bin/bash
#
# DKDM / Config-write 크래시 수정 (EncryptedKDM::as_xml → free_wrappers 세그폴트)
# ============================================================================
# 근본 원인 (2026-07-12 규명, CLI 리프로듀서로 재현·수정 검증):
#   Homebrew 의 libxml++@4 는 formula 에 libxml2 의존성이 없어 시스템
#   /usr/lib/libxml2.2.dylib (libxml2 2.9.x, 구 ABI) 를 링크한다.
#   반면 소스빌드한 libcxml / libdcp / dcpomatic 은 Homebrew keg-only
#   libxml2 (2.15.x, libxml2.16.dylib) 를 링크한다.
#   → 한 프로세스에 libxml2 가 둘 로드된다.
#   dcp::EncryptedKDM::as_xml() 은 libxml++(시스템 libxml2)로 노드를 만든 뒤
#   libdcp 가 xmlAddID() 를 Homebrew libxml2 로 호출 → 서로 다른 libxml2 가
#   같은 Document 를 조작 → Document 파괴 시 xmlpp::Node::free_wrappers 에서
#   EXC_BAD_ACCESS. (플레이어에서 DCP Open → 최근파일 기록 → write_config →
#   DKDMGroup::as_xml 경로로 트리거. EQ 빌드/stock 빌드 공통 — EQ 와 무관.)
#
# 수정: libxml++ 가 Homebrew libxml2 를 쓰도록 install_name 을 바꿔 libxml2 를
#       하나로 단일화한다(install_name_tool + ad-hoc 재서명).
#
# 주의: 이 파일은 Homebrew Cellar 안의 dylib 을 수정하므로
#       `brew reinstall/upgrade libxml++@4` 시 원복된다. 그때 이 스크립트를
#       다시 실행하면 된다(멱등 — 이미 고쳐졌으면 아무 것도 안 함).
# ============================================================================
set -euo pipefail

BREW="$(brew --prefix)"
DYLIB="$BREW/opt/libxml++@4/lib/libxml++-4.0.1.dylib"

if [ ! -f "$DYLIB" ]; then
  echo "오류: libxml++@4 를 찾을 수 없음: $DYLIB" >&2
  echo "      brew install libxml++@4 후 다시 실행하세요." >&2
  exit 1
fi
REAL="$(readlink -f "$DYLIB")"

# 현재 libxml++ 가 링크하는 시스템 libxml2 참조(있으면)
SYS_REF="$(otool -L "$REAL" | awk '/\/usr\/lib\/libxml2.*\.dylib/{print $1; exit}')"

# Homebrew libxml2 실제 versioned dylib (2.16 등 — 버전 변화에도 견고하게 탐색)
HB_REF="$(ls "$BREW"/opt/libxml2/lib/libxml2.*.dylib 2>/dev/null \
          | grep -E 'libxml2\.[0-9]+\.dylib$' | sort -V | tail -1)"
if [ -z "$HB_REF" ]; then
  echo "오류: Homebrew libxml2 dylib 을 못 찾음 ($BREW/opt/libxml2/lib)" >&2
  exit 1
fi

if [ -z "$SYS_REF" ]; then
  echo "✓ 이미 수정됨: libxml++ 가 시스템 libxml2 를 링크하지 않음. (변경 없음)"
  otool -L "$REAL" | grep -i libxml2 || true
  exit 0
fi

echo "수정 대상 : $REAL"
echo "시스템참조: $SYS_REF"
echo "→ 변경   : $HB_REF"

# 최초 1회 백업
if [ ! -f "$REAL.roomtune-bak" ]; then
  cp -p "$REAL" "$REAL.roomtune-bak"
  echo "백업 생성: $REAL.roomtune-bak"
fi

chmod u+w "$REAL"
install_name_tool -change "$SYS_REF" "$HB_REF" "$REAL"
codesign -f -s - "$REAL"   # install_name_tool 이 서명 무효화 → ad-hoc 재서명(arm64 필수)

echo "완료. 현재 링크:"
otool -L "$REAL" | grep -i libxml2

cat <<'NOTE'

검증(선택): dcpomatic 소스트리에서
  clang++ -std=c++17 patches/libxml2-dkdm-crash-fix/verify_kdm.cc ...  (README 참조)
또는 플레이어에서 DKDM 이 등록된 상태로 File > Open DCP → 크래시 없어야 정상.
NOTE
