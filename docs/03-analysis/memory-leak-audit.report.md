# OBS 32.2.2 종료 누수 1건 조사 결과

2026-10-07~08 / 조사 대상 Multiview Plus 0.5.1 Controller / 제작자 SunjooAn

## 결론

관측된 종료 누수 1건은 OBS 기본 rtmp-services.dll의 공통 파일 업데이트 코드에서 HTTP ETag 포인터를 덮어쓰면서 발생한다. 별도 portable OBS의 하단 UCRT allocator 추적으로 **실제 남은 할당 1개, 16바이트**와 호출 경로 `bmemdup → http_header → libcurl → do_http_request`를 확인했다. 멀티뷰가 없는 시험에서도 재현했다.

멀티뷰의 참조·화면·텍스처·자원 감시 스레드 해제 경로를 직접 검토하고 독립 에이전트가 재검토했다. 해당 경로에서 새로운 해제 누락은 찾지 못했다. 이는 모든 종류의 메모리 누수나 장시간 안정성이 증명되었다는 의미는 아니다.

## 원인

공식 OBS 32.2.2 소스 `shared/file-updater/file-updater/file-updater.c`의 http_header에서 ETag 문자열을 할당한 뒤 기존 etag_remote를 해제하지 않고 교체한다(로컬 원본 89, 99줄). 같은 CURL handle로 package.json 후 services.json 등을 다운로드하며 이전 헤더 콜백이 유지된다. 추가 파일의 ETag가 package ETag 포인터를 덮어쓴다. update_thread는 마지막 ETag만 해제한다(450줄).

캐시가 없는 최초 실행에서 추가 파일 다운로드가 발생해 1건이 남고, 초기화 후 재실행에서 파일 다운로드가 생략되면 0건이 되는 비교 결과와 일치한다. 이 조건은 네트워크 응답과 서버 파일 버전에 따라 바뀔 수 있다. 최초 실행에만 제한된 오류라고 일반화하지 않는다.

공식 소스: https://github.com/obsproject/obs-studio/blob/32.2.2/shared/file-updater/file-updater/file-updater.c

## 검증

| 시험 | 결과 |
|---|---|
| 멀티뷰 DLL 없는 새 portable 설정 | 종료 누수 1건 |
| 멀티뷰 로드만 / 창 열기, 새 설정 | 각각 1건 |
| UCRT aligned allocator 추적, 새 설정 | OBS count 1, 실제 미해제 1개 16바이트 |
| 동일 설정 재실행, 하단 추적 | OBS count 0, 실제 미해제 0개 |
| 원본 OBS DLL 재실행, 추적 도구 없음 | 종료 누수 0건 |
| 초기화 후 전체 멀티뷰 + Scene Anchor + Source Switcher 시험 | 기능 검사 통과, 종료 누수 0건, 잔존 소스·중복 파괴 없음 |
| 동일 환경 멀티뷰 열기·닫기 40회 | 40/40 통과, 종료 누수 0건 |
| 공식 HTTP callback 재현과 수정안 1,000회 | 원본 재현 1건, 수정안 0건, package ETag 유지 |

40회 시험의 창 닫힘 후 libobs 할당 수는 15,258~15,262, 마지막 15,258로 반복에 따른 증가가 관측되지 않았다. 처음 창을 만들 때 정상적으로 유지하는 객체의 할당은 종료 때 모두 해제되었다. Qt/Windows heap 전체 및 장시간 방송은 이 수치만으로 평가할 수 없다.

전체 기능 시험: `.local/runtime/run-20261007-235636/results`
40회 시험: 같은 실행 폴더의 `stress-results`
원주소 추적: `.local/leak-trace/cold-crt.txt`, `warm-crt.txt`
공식 callback 추출 재현 코드: `.local/leak-trace/etag-regression.cpp`
주요 증거는 `docs/03-analysis/evidence/memory-leak/`에도 보관했다.

## 수정안과 적용 범위

`docs/patches/obs-32.2.2-file-updater-etag.patch`에 OBS 원본용 수정안을 작성했다. 자식 파일 헤더는 무시하고 같은 package 응답에서 ETag를 다시 받으면 이전 값을 해제한다. 공식 callback을 추출한 독립 실행 검증에서 1,000회 반복 후 미해제 0, package ETag 보존을 확인했다. **패치를 적용한 rtmp-services DLL 전체를 빌드·실행한 결과는 아니다.**

이 패치는 OBS upstream 검토용이며 현장 OBS·기본 모듈에는 적용하지 않았다. 멀티뷰에서 외부 updater의 사유 포인터를 임의로 해제하거나 네트워크 기능을 차단하는 우회는 넣지 않았다. 외부 라이브러리 제작자·라이선스는 유지한다. 0.4.0, 0.5.0 및 현장 설치본도 변경하지 않았다.

시험 도구는 초기화 실행과 본 시험의 로그를 분리하는 `-InitializeBeforeTest`를 추가했다. 기본 종료 누수 기대값은 여전히 0이다. 최초 누수 1건을 성공으로 숨기지 않고 경고·별도 로그로 남긴다. `-LeakProbeMode window -LeakCycles 40`으로 반복 시험을 재현할 수 있다. 진단 proxy·MinHook은 `.local` 안의 폐기 가능한 portable 복사본에서만 사용하며 배포하지 않는다.

## 방송 안정성 판단

이번 16바이트 문자열 유실은 방송 프레임마다 발생하는 멀티뷰 누적 누수 경로가 아니다. 다운로드 헤더를 교체할 때 발생한다. 이 1건만으로 즉시 메모리 고갈이나 방송 중단을 예상할 근거는 없으나, 업데이트 요청이 반복되면 다시 발생할 수 있으므로 무해하다고 단정하지 않는다. 현재 시험에서는 멀티뷰의 반복 사용 누수는 관측되지 않았다. 장시간 실제 카메라·방송·드라이버 조합 검증은 별도 범위다.

패치 문맥 확인: 로컬 Windows 원본의 개행 차이를 허용한 git apply --check --ignore-space-change 통과. 실제 적용은 하지 않음.

추가 확인: 최종 시험 스크립트의 -InitializeBeforeTest -LeakProbeMode window -LeakCycles 40 실행도 통과(.local/runtime/run-20261008-000121). 첫 실행 누수1은 별도 경고·로그로 보관하고 본 실행 종료0을 확인했다. 빌드 DLL은 기존 release/0.5.1 DLL과 SHA256이 동일하다.

독립 최종 검토: 이번 package ETag 덮어쓰기와 패치 범위는 일치한다. 공통 updater의 single_file_thread는 별도 ETag 정리 경로 검토 후보이며 이번 16바이트 실행에서 해당 경로를 재현한 증거는 없다. 이 수정안이 updater의 모든 누수를 해결한다고 주장하지 않는다.
