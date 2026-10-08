# Sunjoo OBS Link Multiview

제작자: SunjooAn · 컴포넌트 버전: 0.5.3

Sunjoo OBS Link는 Controller, Multiview, Tally가 함께 사용하는 제품군입니다.
Controller 0.1.1, Multiview 0.5.3, Tally 1.6.0-obs.5 조합으로 검증합니다.
기존 source type ID, 설정 경로, WebSocket vendor 및 카메라 배정은 호환성을 위해 유지합니다.
기존 최종 비 Hybrid 버전은 별도로 보관하며 방송 중 자동 설치하지 않습니다.
리스너 펌웨어, UDP, vMix/ATEM에는 변경이 없습니다.

설치는 방송이 없는 시간에 OBS를 종료한 후 진행합니다. 기존 DLL과 같은 파일명으로 교체하여 모듈의 중복 로드를 방지하세요. Controller Lua는 camera-mix-hybrid.lua를 사용하며 기존 camera-mix-controller.lua와 같은 출력에 동시 적용하지 마세요.

## 검증 결과 (2026-10-08)
- 격리 OBS 32.2.2에 세 모듈 동시 로드: PGM 1/PVW 2 및 역방향 장면·카메라 소스 적색/녹색 판정 일치, 정상 종료.
- ME1/ME2 실제 libobs 복제, 배치 유지, CUT/MIX 25/50/75% 밝기·고정 자막 검증 통과.
- Controller 500 복제: 할당 13680→13680. Multiview 2000 조회: 13908→13908. Tally 2000 조회: 8414→8414.
- Controller 생성/반복 적용/종료/재시작 복원 통과. Tally 요청 중 5회+유휴 1회 정상 종료.
- Multiview CTest 2종, 탈리 Jest 27 suite/273 성공/기존 1 skip.
- Multiview 설정/전체 화면/재열기/종료 검증 통과. 초기 OBS RTMP 기본 구성 누수 1건, 초기화 프로필 종료 0건. 프로그램 전체가 모든 환경에서 무누수라고 보장하는 결과는 아님.
- 패키징 허브의 기존 OBS 방식 및 native 옵션, MIX 양쪽 카메라, 이벤트 500개 병합, 장면 모음 변경, 재접속, 배정 저장 회귀 검증 통과.

## 개선
Multiview 그래프 재귀를 반복 탐색으로 변경하고 8192 노드 제한을 적용했다. 깊은 중첩, 순환, 한도 초과 회귀를 추가했다. 테스트 허브 UDP는 OS 자동 포트를 사용해 현장 7411과 충돌하지 않도록 했다. 화면/OBS 모듈 표시 이름을 통일했고 저장된 식별자는 유지했다.

## 사용과 보존
기존 입력 배정은 유지한다. 기존 허브는 native 실제 경로 옵션 ON일 때만 새 bridge의 복제 판정을 사용한다. OFF는 기존 방식이다. 리스너 firmware 업데이트는 필요하지 않다. 실제 장치/현장 장시간 운용 테스트와 서명은 수행하지 않았다. 현재 설치본은 변경하지 않았다.
