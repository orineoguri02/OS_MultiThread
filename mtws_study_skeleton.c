/*
 * 이 파일은 기능별 모듈로 분리되었습니다.
 *
 *   include/mtws_types.h  — 공유 자료구조
 *   include/args.h        — 명령행 인자
 *   include/buffer.h      — bounded buffer
 *   include/dirwalk.h     — 디렉토리 탐색 (producer)
 *   include/search.h      — 단어 검색
 *   include/worker.h      — worker thread
 *   src/main.c            — 프로그램 진입점
 *   src/*.c               — 위 헤더에 대응하는 구현
 *
 * 빌드: make
 * 실행: ./mtws_study -b 5 -t 3 -d ./dir1 -w hello
 *
 * TODO는 각 src/*.c 파일에 있습니다.
 */

#error "Use 'make' and src/main.c — this monolithic file is no longer compiled."
