# DKDM / Config-write 크래시 수정 (libxml2 이중 로드)

## 증상
플레이어에서 DKDM 이 config 에 등록된 상태로 **File > Open DCP** 를 하면 세그폴트.
크래시 스택:

```
xmlpp::Node::free_wrappers        ← EXC_BAD_ACCESS @ 0x8
xmlpp::Document::~Document
dcp::EncryptedKDM::as_xml() const
DKDM::as_xml → DKDMGroup::as_xml
Config::write_config
DOMFrame::config_changed → add_to_player_history → load_dcp → file_open
```

## 근본 원인 (2026-07-12 규명)
한 프로세스에 **libxml2 가 두 개** 로드된다:

| 라이브러리 | 링크하는 libxml2 |
|---|---|
| Homebrew `libxml++@4` (bottle, formula 에 libxml2 의존성 없음) | 시스템 `/usr/lib/libxml2.2.dylib` (**2.9.x**, 구 ABI) |
| 소스빌드 `libcxml` / `libdcp` / `dcpomatic` | Homebrew `libxml2.16.dylib` (**2.15.x**, 신 ABI) |

`dcp::EncryptedKDM::as_xml()`(`libdcp/src/encrypted_kdm.cc`)은 libxml++(시스템 libxml2)로
노드를 만든 뒤, `xmlAddID(0, document->cobj(), ...)` 를 libdcp 가 링크한 Homebrew libxml2 로
호출한다. 서로 다른 libxml2 가 같은 Document 를 조작 → Document 파괴 시 `free_wrappers`
재귀에서 잘못된 노드 포인터를 역참조 → 크래시.

**EQ 빌드 한정이 아니다.** EQ 빌드와 stock 빌드의 libxml2 링크는 동일하며, 크래시 재현기에는
EQ 코드가 한 줄도 없다. 두 빌드 공통 문제.

## 수정
`libxml++` 가 Homebrew libxml2 를 쓰도록 install_name 을 바꿔 libxml2 를 **하나로 단일화**한다.

```bash
bash apply-fix.sh
```

- 멱등: 이미 고쳐졌으면 아무 것도 안 함.
- 최초 1회 `*.roomtune-bak` 백업 생성.
- `install_name_tool` 후 ad-hoc 재서명(arm64 필수).

두 플레이어 빌드(EQ·stock)가 같은 libxml++ 를 링크하므로 한 번에 둘 다 고쳐진다.
이후 dcpomatic 을 재빌드해도 이 수정은 유지된다(libxml++ dylib 자체를 고쳤으므로).

> ⚠️ Homebrew Cellar 안의 dylib 을 수정하므로 `brew reinstall/upgrade libxml++@4`
> 시 원복된다. 그때 `apply-fix.sh` 를 다시 실행하면 된다.

## 검증 (GUI 없이)
`verify_kdm.cc` 는 실제 크래시 함수 `EncryptedKDM::as_xml()` 를 CLI 에서 직접 호출한다.

```bash
# libdcp 테스트 KDM 으로 재현/검증 (컴파일 플래그는 pkg-config libdcp-1.0 사용)
PKG_CONFIG_PATH=~/dcpomatic-env/lib/pkgconfig:/opt/homebrew/opt/libxml2/lib/pkgconfig:/opt/homebrew/lib/pkgconfig \
clang++ -std=c++17 verify_kdm.cc -o verify_kdm \
  $(pkg-config --cflags libdcp-1.0) -I$HOME/dcpomatic-env/include/libdcp-1.0 \
  $(pkg-config --libs libdcp-1.0) -L/opt/homebrew/lib -L/opt/homebrew/opt/boost/lib -lboost_filesystem
./verify_kdm ~/src/libdcp/test/data/other_kdm.xml
```

- 수정 전: `constructed EncryptedKDM` 출력 직후 SIGSEGV(종료코드 139).
- 수정 후: `as_xml()` 5회 모두 성공 후 `DONE (no crash)`(종료코드 0).

## 근본적 대안(선택)
Homebrew Cellar 수정이 껄끄러우면, `libxml++@4` 를 소스에서 Homebrew libxml2 에
링크되도록 빌드하거나(formula 수정 필요), libcxml/libdcp/dcpomatic 을 시스템 libxml2
2.9.x 에 맞춰 재빌드하는 방법이 있다. 현재 수정이 가장 적은 변경으로 두 빌드를 모두 고친다.
