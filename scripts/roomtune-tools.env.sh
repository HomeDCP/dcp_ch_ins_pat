#!/bin/bash
# RoomTune 빌드 도구를 "같은 맥의 다른 프로젝트"에서 참조하기 위한 환경.
#
# 사용법:  source /Users/homedcp/Claude/Projects/roomtunning/scripts/roomtune-tools.env.sh
#          roomtune-player "/경로/DCP폴더"     # Room EQ 포함 Player (View 메뉴에 토글)
#          roomtune-cli --help
#          "$ROOMTUNE_TOOLS/dcpomatic2_verifier" ...   # 그 외 도구 직접 호출
#
# ★ 주의: 이 바이너리들은 dylib와 리소스를 '절대경로'로 참조하는 개발 빌드다.
#   - 반드시 제자리($ROOMTUNE_TOOLS)에서 실행할 것. 바이너리만 딴 데로 복사하면 안 됨.
#   - 빌드 디렉토리(~/src/dcpomatic/build)를 옮기거나 `./waf distclean` 하면 동작 안 함.
#   - 다른 '머신'에서 쓰려면 자립 번들 패키징 또는 레시피 재빌드 필요(docs 참조).

export ROOMTUNE_TOOLS="$HOME/src/dcpomatic/build/src/tools"

roomtune-player()   { "$ROOMTUNE_TOOLS/dcpomatic2_player"   "$@"; }   # ← 룸 EQ 포함
roomtune-cli()      { "$ROOMTUNE_TOOLS/dcpomatic2_cli"      "$@"; }
roomtune-verify()   { "$ROOMTUNE_TOOLS/dcpomatic2_verifier" "$@"; }
roomtune-create()   { "$ROOMTUNE_TOOLS/dcpomatic2_create"   "$@"; }

if [ -x "$ROOMTUNE_TOOLS/dcpomatic2_player" ]; then
  echo "[roomtune-tools] ROOMTUNE_TOOLS=$ROOMTUNE_TOOLS (도구 $(ls "$ROOMTUNE_TOOLS"/dcpomatic2_* 2>/dev/null | grep -vc '\.o$')종)"
else
  echo "[roomtune-tools] 경고: $ROOMTUNE_TOOLS 에 빌드 산출물이 없습니다. 빌드가 필요합니다."
fi
