#!/bin/bash
# 소스빌드한 DCP-o-matic 도구를 "같은 맥의 다른 프로젝트"에서 참조하기 위한 환경.
#
# 두 가지 빌드가 존재한다:
#   • 정식(stock) 빌드  → EQ 없음. 일반 용도 / GPU J2K 인코딩 프로젝트의 검증·인코딩 베이스.
#   • 룸튜닝(EQ) 빌드    → 재생 경로에 룸 EQ가 들어간 변형(별개 프로젝트).
#
# 사용법:  source /Users/homedcp/Claude/Projects/roomtunning/scripts/roomtune-tools.env.sh
#          dcpm-player "/경로/DCP폴더"     # 정식 Player (중간산출물 확인용)
#          dcpm-cli --help                 # 정식 인코딩 CLI
#          "$DCPOMATIC_TOOLS/dcpomatic2_verifier" ...
#
# ★ 주의: 개발 빌드라 dylib·리소스를 '절대경로'로 참조한다.
#   - 반드시 제자리에서 실행. 바이너리만 복사 금지. build 디렉토리 이동/`waf distclean` 금지.
#   - 다른 '머신'에서 쓰려면 재빌드(레시피) 또는 자립 번들 필요.

# ── 정식(stock) 빌드: 다른 프로젝트가 쓸 기본 ──
export DCPOMATIC_TOOLS="$HOME/src/dcpomatic-stock/build/src/tools"
# ── 룸튜닝(EQ) 빌드 ──
export ROOMTUNE_TOOLS="$HOME/src/dcpomatic/build/src/tools"

# 정식 도구 헬퍼 (다른 프로젝트용)
dcpm-player()  { "$DCPOMATIC_TOOLS/dcpomatic2_player"   "$@"; }   # 중간산출물 확인
dcpm-cli()     { "$DCPOMATIC_TOOLS/dcpomatic2_cli"      "$@"; }   # 인코딩 CLI
dcpm-verify()  { "$DCPOMATIC_TOOLS/dcpomatic2_verifier" "$@"; }
dcpm-create()  { "$DCPOMATIC_TOOLS/dcpomatic2_create"   "$@"; }
dcpm-server()  { "$DCPOMATIC_TOOLS/dcpomatic2_server_cli" "$@"; } # 분산 인코딩 서버

# 룸튜닝(EQ) Player 헬퍼
roomtune-player() { "$ROOMTUNE_TOOLS/dcpomatic2_player" "$@"; }

if [ -x "$DCPOMATIC_TOOLS/dcpomatic2_player" ]; then
  echo "[dcpomatic-tools] 정식: $DCPOMATIC_TOOLS  ($(ls "$DCPOMATIC_TOOLS"/dcpomatic2_* 2>/dev/null | grep -vc '\.o$')종)"
else
  echo "[dcpomatic-tools] 경고: 정식 빌드가 아직 없습니다 → $DCPOMATIC_TOOLS"
fi
[ -x "$ROOMTUNE_TOOLS/dcpomatic2_player" ] && echo "[dcpomatic-tools] 룸튜닝(EQ): $ROOMTUNE_TOOLS"
