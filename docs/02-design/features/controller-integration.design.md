# Controller 경로 설계
기존 scene/group은 scene item별 visible 경로를 유지한다. 이외 소스는 obs_source_enum_active_sources를 사용한다.
Source Switcher enum_active_sources는 current transition과 current source를 노출한다. enum_all_sources는 미선택 입력도 포함하므로 사용하지 않는다.
OBS32.2.2는 transition의 active children 열거에서 private A/B를 제공한다. private scene view -> 공유 group -> 카메라 원본으로 재귀한다.
콜백 안에서는 자식의 강한 참조를 수집하고 콜백 반환 후 탐색하여 부모 잠금 안에서 자식 잠금을 중첩하지 않는다.
출력의 숨겨진 보관 scene과 view의 동일 group은 edge별 visibility로 구분한다. 전역 is_active/is_showing은 탈리 기준으로 쓰지 않는다.
단일 영상 leaf 자동 매핑과 명시 UUID 매핑 유지. active children이 있는 wrapper를 raw leaf로 오인하지 않도록 한다.
UI에 Controller 버전 표기. 기존 설정 스키마와 클릭/프리셋 유지. 카메라 제어 UI 추가는 이번 범위에 불필요.
테스트: native private Fade, source-switcher.dll 실물, 숨긴 부모/여러 출력 경로/이름 변경/엔진 교체/장면 재로드, 기존 전체 smoke.
현장 설치/외부 탈리 작업/자동 GitHub 공개는 금지. 로컬 release/0.5.0만 생성.
