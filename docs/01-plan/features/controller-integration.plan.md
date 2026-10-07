# Camera MIX Controller 연동 계획
2026-10-07 / 0.5.0 Controller / feature/camera-mix-controller-0.5.0

목표: 기존 0.4.0 기능 위에 Camera MIX Controller 1.3.1의 ME1 Source Switcher와 ME2~ME8 private Fade 출력 경로를 추가 지원.
범위: 실제 PGM/PVW의 보이는 장면/그룹과 native active child 탐색. UUID 기준 소스 매핑 유지. MIX 중 이전/새 카메라, 완료 후 이전 해제.
이름이나 ME 선택값만으로 방송 여부를 추정하지 않음. 숨긴 보관 장면의 전체 입력을 열거하지 않음.
완료 조건: 실제 native Fade 및 Source Switcher 격리 시험, 기존 레이아웃/탈리 회귀, 별도 릴리즈 폴더와 버전 UI.
보존: 기존 0.4.0 배포/현장 설치. 현장 OBS 종료/재시작/교체 금지. GitHub 공개 릴리즈 금지.
순서: 멀티뷰 구현/검증/로컬 배포본 완료가 먼저. 외부 탈리 허브/리스너 확장은 보류. 해당 코드/현장 서비스 수정 금지.
사용량: 단계별 체크포인트 기록. 리셋 크레딧/유료 추가 사용 자동 소비 금지.
