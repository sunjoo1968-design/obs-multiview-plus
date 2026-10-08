# Sunjoo OBS Link Multiview

제작자: SunjooAn · 컴포넌트 버전: 0.5.3

Sunjoo OBS Link는 Controller, Multiview, Tally가 함께 사용하는 제품군입니다.
Controller 0.1.1, Multiview 0.5.3, Tally 1.6.0-obs.5 조합으로 검증합니다.
기존 source type ID, 설정 경로, WebSocket vendor 및 카메라 배정은 호환성을 위해 유지합니다.
기존 최종 비 Hybrid 버전은 별도로 보관하며 방송 중 자동 설치하지 않습니다.
리스너 펌웨어, UDP, vMix/ATEM에는 변경이 없습니다.

설치는 방송이 없는 시간에 OBS를 종료한 후 진행합니다. 기존 DLL과 같은 파일명으로 교체하여 모듈의 중복 로드를 방지하세요. Controller Lua는 camera-mix-hybrid.lua를 사용하며 기존 camera-mix-controller.lua와 같은 출력에 동시 적용하지 마세요.
