# OBS Multiview Plus 0.5.2 Hybrid

제작자 **SunjooAn** · 2026-10-08 · OBS Studio 32.2.2 / Windows x64

## 결과

Camera MIX Hybrid 0.1.0의 소스 복제 ON에서 ME1 장면·소스 타일의 PGM 적색과 Preview 녹색을 복원했습니다. 실제 방송 출력에서 판정하고 복제본의 원본 UUID로 배정 대상을 대응하며, Preview 변경은 복제 PGM 상태를 바꾸지 않습니다. CUT의 이전 경로 잔존도 보정했습니다.

기존 ME2, Source Switcher, 명시적 탈리 소스 지정, 레이아웃 JSON/UUID를 유지합니다. 기존 릴리스와 현장 설치본은 변경하지 않았습니다. 이번 작업에서 탈리 프로그램·펌웨어는 수정하지 않았습니다.

## 검증

- MSVC x64 Release 빌드, CTest 레이아웃·탈리 그래프 2개 통과. 탈리 그래프 34 scenarios.
- 실제 멀티뷰 창: ME1 PGM 장면/소스 1 적색·Preview 2 녹색, 반대 조합도 통과. 정상 종료 exit=0.
- 실제 libobs: ME1 장면·소스·명시적 대응, ME2, 내부 이름 충돌, Preview가 숨긴 원본과 독립된 PGM 가시성 검증.
- 2000회 native 조회 전후 OBS 할당 수 13908 → 13908. Source Toggler의 실제 복제/CUT/MIX 픽셀 검사도 통과.
- 기존 전체 UI·영상 캡처·설정 수락·창 재열기·Controller/Source Switcher 런타임 검사 통과.
- 초기화 후 전체 런타임 종료 누수 0. Cold start와 Hybrid 창 별도 런타임의 OBS 전체 누수 수 1은 기존 baseline과 일치하며, 전체 OBS 무누수로 주장하지 않습니다. 기존 memory-leak-audit.report.md에 조사 범위가 기록돼 있습니다.

## 설치

방송 외 시간에 OBS를 종료하고 새 설치 ZIP의 obs-plugins/data를 OBS 설치 폴더에 합친 뒤 재실행합니다. 기존 레이아웃과 타일 UUID는 유지됩니다. Hybrid와 OBS의 장면 복제/소스 복제를 켜고 기존 카메라 장면·소스 타일로 확인합니다. 명시적 탈리 소스 선택도 그대로 사용할 수 있습니다.

현장 설치와 GitHub 게시를 자동 수행하지 않았습니다. 기존 0.5.1은 별도 릴리스 폴더에 보존합니다. 테스트 모듈 DLL, 로컬 재개 문서, 개인 설정, 의존성·로그는 설치/공개 소스 패키지에서 제외합니다.

## 재검증

공식 의존성 준비 후 scripts/build.ps1, scripts/test-runtime.ps1 -WithSourceSwitcher -InitializeBeforeTest -ExpectedMemoryLeaks 0을 사용합니다. Hybrid native 검사는 tests/test-hybrid-native.py에 MV_HYBRID_NATIVE_FIXTURE와 MV_HYBRID_NATIVE_PROBE를 지정합니다.

실제 Hybrid 타일 검사는 scripts/prepare-hybrid-test.ps1로 격리 runtime을 구성한 뒤, MV_HYBRID_LUA에 camera-mix-hybrid.lua 경로, MV_TEST_PYTHON에 Python 실행 파일을 지정하여 node scripts/test-hybrid-runtime.cjs를 실행합니다. Node.js 22+와 설치된 OBS/Source Switcher/Hybrid가 필요합니다.
