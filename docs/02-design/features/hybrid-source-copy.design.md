# Hybrid 소스 복제 호환 설계

버전 원본은 CMake PROJECT_VERSION, 표시 이름은 src/version.hpp의 Hybrid/SunjooAn입니다. 기존 layout.json version 2와 UUID/tallyUuid를 그대로 사용합니다.

PGM 판정은 obs_get_output_source(0)의 실제 방송 출력에서 시작합니다. 원본 장면으로 대체하지 않으며 fallback은 방송 출력이 없을 때만 사용합니다.

sourceOnRoot는 children 참조를 확보한 뒤 열거기 lock 밖에서 탐색합니다. 바깥 Studio 복제 장면과 Hybrid가 기록한 camera_mix_hybrid_original_uuid만 원본 대응의 근거로 사용합니다. 원본 장면 내부의 scene-item ID 또는 유일한 이름/타입으로 하위 소스를 대응하되, 탐색은 복제본의 가시성에서 계속합니다. 이름만 같은 내부 private MIX view는 임의 대응하지 않습니다. CUT은 실제 active target만 포함하고 Fade의 양쪽 영상 기여 판단을 유지합니다.

그래프 비교는 original identity를 비교하는 match 함수를 분리합니다. 실제 탐색 노드를 원본으로 치환하지 않아 Preview 변경이 PGM 표시를 바꾸지 않습니다. 명시적 tallyUuid는 자동 대응보다 우선합니다. 참조는 쿼리 종료 시 회수하며 native 탐색·대응 크기를 제한합니다.

실제 창 검사용 hybrid-probe는 portable flag와 전용 출력 경로가 있을 때만 실행하며 배포 설치 ZIP에 넣지 않습니다. 별도 프로필·임시 인증/포트·테스트 카메라를 사용하고 생성한 OBS 프로세스 경로/PID를 검증한 뒤 그 본창만 정상 종료합니다.
