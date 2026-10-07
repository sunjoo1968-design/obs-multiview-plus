# OBS Multiview Plus 0.5.1 Controller

제작자 **SunjooAn** · 버전 **0.5.1 Controller** · Windows x64 / OBS Studio **32.2.2**

Camera MIX Controller 출력 경로 연동과 제작자·버전 상시 표시를 포함한 정식 릴리즈입니다. 기존 0.4.0 안정 버전과 0.5.0 Controller 사전 릴리즈는 그대로 보존합니다.

## 변경 사항

- Camera MIX Controller의 중첩 장면·그룹·활성 소스를 따라 실제 프로그램/프리뷰 포함 여부를 확인합니다.
- Source Switcher와 private Fade MIX의 실제 영상 기여 구간을 반영합니다.
- 창 제목, 설정창, 전체 화면의 하단 상태 표시에서 SunjooAn과 현재 버전을 함께 표시합니다.
- 기존 ATEM형 배치, 드래그 위치 교환, 이름 오버레이, PGM/PVW 색상, 클릭 전환 옵션 및 정보 칸을 유지합니다.
- 메모리 누수 비교·반복 사용 시험 도구와 조사 증거를 정리했습니다.

## 검증 및 메모리 누수 조사

- Release 빌드와 CTest 2/2 통과.
- 초기화 후 별도 portable OBS에서 전체 기능 및 Scene Anchor·Source Switcher 동시 시험 통과, 종료 누수 0건.
- 멀티뷰 열기·닫기 40회 시험 통과, 종료 누수 0건. 반복에 따른 libobs 할당 증가가 관측되지 않았습니다.
- 새 설정에서 나타난 종료 누수 1건은 OBS 기본 rtmp-services 모듈의 HTTP ETag 포인터 덮어쓰기로 확인했습니다. 실제 미해제 할당은 16바이트 1개이며 멀티뷰가 없는 OBS에서도 재현됩니다.
- 해당 OBS callback 수정안을 독립 재현 시험에서 1,000회 검증했습니다. OBS 원본용 패치는 참고 자료이며 설치본에 적용하지 않았습니다. 수정한 OBS 기본 DLL 전체의 빌드·실행 검증은 포함하지 않습니다.

[메모리 조사 보고서](https://github.com/sunjoo1968-design/obs-multiview-plus/blob/v0.5.1/docs/03-analysis/memory-leak-audit.report.md) · [Controller 검증 보고서](https://github.com/sunjoo1968-design/obs-multiview-plus/blob/v0.5.1/docs/04-report/controller-integration.report.md)

## 설치

1. `obs-multiview-plus-0.5.1-windows-x64.zip`을 내려받습니다.
2. OBS를 종료하고 기존 플러그인을 백업합니다.
3. ZIP의 `obs-plugins`와 `data`를 OBS 설치 폴더에 합칩니다. 기본 위치는 `C:\Program Files\obs-studio`입니다.
4. OBS를 실행하고 **도구 → Multiview Plus**를 선택합니다.

ZIP에는 멀티뷰 DLL과 안내·라이선스만 포함합니다. OBS/Qt DLL과 시험용 DLL은 포함하지 않습니다. 기존 사용자 배치는 유지됩니다. 배포 파일의 SHA256은 `SHA256SUMS.txt`로 확인할 수 있습니다.

## 검증 범위

실제 Lua 컨트롤러 전체 조작 및 장시간 현장 방송 검증은 별도입니다. 일반 픽셀 가림·필터 투명도까지 탈리에 계산하지 않습니다. 외부 탈리 허브·리스너 확장은 보류 상태이며 해당 프로그램은 수정하지 않았습니다. 현장 설치본은 자동 교체하지 않습니다.

기존 버전: [0.4.0](https://github.com/sunjoo1968-design/obs-multiview-plus/releases/tag/v0.4.0) · [0.5.0 Controller](https://github.com/sunjoo1968-design/obs-multiview-plus/releases/tag/v0.5.0-controller)
