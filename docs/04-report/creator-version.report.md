# 제작자 및 버전 표시 — 0.5.1 Controller
제작자: **SunjooAn** / 버전: **0.5.1 Controller**.

창 제목과 항상 보이는 하단 상태 표시, 설정창에 제작자와 버전을 함께 표시합니다.
전체 화면에서도 상태 표기를 유지합니다. 버전 값은 CMake의 PROJECT_VERSION에서 전달하여 UI/모듈/로그가 같은 버전을 사용합니다.
README와 배포 안내에 동일 표기를 기록했으며 외부 OBS/Qt 및 GPL 제작자/라이선스 표기는 유지합니다.

Release 빌드 및 CTest 2/2 통과. 격리 OBS 시험 .local/runtime/run-20261007-231735/results.
설정창/전체 화면에서 creatorVersionLabel 가시성과 정확한 제작자·버전 문자열 검사가 통과했습니다.
기존 controller/SourceSwitcher/씬트리/레이아웃/장면 모음 복귀 회귀도 통과했습니다.
종료 누수1건은 기존 비교 기준과 동일하며 해당 미확정 한계는 유지됩니다.

기존0.4.0 및0.5.0 배포는 보존. release/0.5.1에 별도 배포본 생성. 현장 자동 설치와 GitHub 새 공개는 수행하지 않았습니다.
외부 탈리 시스템 개선은 계속 보류합니다.
