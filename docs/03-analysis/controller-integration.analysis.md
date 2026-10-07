# Controller 0.5.0 구현/설계 비교
2026-10-07. 계획의 출력 경로 탐색, 영상 MIX 기여, UUID 매핑, 숨김 경로, 기존 기능 유지 항목 구현.

- 기존 scene/group visible edge 유지, 이외 native active children 추가.
- 콜백은 강한 자식 참조만 수집, 콜백 반환 후 탐색. 판정별 캐시이므로 재로드 이후 상태를 보관하지 않음.
- Fade A/B는 OBS 열거 잠금 안에서 읽은 영상 시간으로 시작/중간/완료 기여 판정. 오디오 tail의 이전 소스 오탐 해소.
- Source Switcher는 active enum만 사용. 실제 설치 DLL의 CUT/MIX와 GPU 혼합 픽셀 시험 통과.
- 이름 변경과 private 엔진 재생성, 장면 모음 전환/복귀 및 UUID 복원 통과.
- 기존 프리셋/드래그/클릭 설정 코드 유지. 기존 회귀/프로그램 색상 우선 검사 통과.
- 가짜 장면/신호 소스 생성, ME 선택 조작, 외부 탈리 변경 없음.

최종 시험 .local/runtime/run-20261007-224851. 기능 검사 전체 통과.
종료 시 bmalloc 누수1. 기존0.4.0 비교 실행(run-20261007-224641)도1. 발생 주체 미확정, 누수0이라고 표시하지 않음.
시험 전용 공유 private 그룹 정리 보완 후 source remaining/Double destroy 경고 없음.
장시간 실방송 및 실제 Lua 스크립트 UI 전체 적용/자동재로드 흐름은 아직 현장 검증 대상.
