# OBS Multiview Plus 0.5.0 Controller 검증 보고서
2026-10-07 / feature/camera-mix-controller-0.5.0 / 로컬 검증 배포.

## 결과
ME1 Source Switcher 및 ME2~ME8 private Fade 출력의 카메라를 실제 PGM/PVW 경로에서 추적하도록 기존 멀티뷰를 확장했습니다.
MIX 중 양쪽 카메라를 표시하고 영상 완료 후 이전 카메라를 해제합니다. 숨긴 보관 장면/방송 밖 ME는 제외합니다.
기존 탈리 기준 소스 UUID/단일 카메라 자동 매핑과 프로그램 우선 색상, 레이아웃/프리셋/클릭/정보 칸을 유지합니다.
사용자 장면 이름을 추정하거나 별도 탈리 신호 소스를 생성하지 않습니다.

## 수정과 안정성
기존 탐색은 scene/group까지만 처리해 private 전환 출력에서 카메라를 놓쳤습니다.
이제 native active children을 강한 참조 snapshot으로 수집하고 native 잠금 밖에서 탐색합니다.
OBS Fade는 영상 완료 후 오디오 처리 때문에 A/B 참조가 남을 수 있습니다. Fade 영상 진행률로 기여를 제한해 이전 탈리 잔존을 수정했습니다.
다른 전환 종류는 각 소스가 제공하는 active children 경로를 유지합니다.

## 확인한 시험
- Release 빌드, CTest layout-model/tally-graph 2/2 통과.
- 실제 private native Fade/group/view fixture: 선택/숨김/방송 밖 선택/여러 경로/명시 매핑/이름 변경/엔진 재생성/MIX 진행 및 완료 통과.
- 실제 설치 Source Switcher DLL: current only/CUT/MIX 기여 및 종료, 숨긴 부모, scene→raw 매핑, 실제 GPU 혼합 픽셀 통과.
- OBS 장면 모음 실제 교체/복귀: 이전 UUID 해제와 같은 UUID 복원, 타일 재개 통과.
- 기존 드래그/프리셋/전체화면/재열기/통계/시계/CPU-GPU 상태 회귀 통과.
- 씬트리 초기/설정후/재열기/장면 모음 복귀 모두9/9 일치.
최종: .local/runtime/run-20261007-224851/results. 공개 가능한 검사 요약은 docs/03-analysis/evidence/controller/result.json.

## 종료 검사와 한계
종료 메모리 누수 보고는1건입니다. 동일 환경의 기존0.4.0 비교 시험(run-20261007-224641)도1건이어서 새 버전의 증가로 확인되지 않았습니다. 발생 주체는 확정하지 못했습니다.
초기 시험의 private 공유 그룹 정리 문제는 시험 fixture에서 보완했고 최종에는 남은 source나 Double destroy 경고가 없습니다.
기본 test-runtime은 누수0을 요구합니다. 이 환경의 비교 시험은 -ExpectedMemoryLeaks 1을 명시해 같은 기준인지 확인하며 경고를 출력합니다. 검사 기준을 몰래 완화하지 않습니다.
실제 Camera MIX Controller Lua 전체 UI/자동 적용 흐름은 직접 실행하지 않았습니다. 해당 코드 구조를 읽고 native fixture로 재현한 출력 경로와 실제 Source Switcher를 검증했습니다.
장시간 방송, 모든 카메라 장치/필터/드라이버, native 영상 위 이름 상자 최종 외관은 현장 적용 전 확인해야 합니다.

## 배포 및 순서
release/0.5.0에 설치 ZIP/DLL/소스 ZIP/설명서/체크섬이 있습니다. 창/모듈/로그에0.5.0 Controller 표기.
기존 release/0.4.0 DLL SHA256 FB0DD96DE161AF40885964C134BEA8F2AAD7A3B77542E34CF18DE2BD13D5775C 보존 확인.
현장 설치 교체/OBS 종료/재시작/GitHub 공개를 수행하지 않았습니다.
외부 탈리 허브·리스너 개선은 계속 보류입니다. 이 결과로 자동으로 해당 작업을 착수하지 않습니다.
