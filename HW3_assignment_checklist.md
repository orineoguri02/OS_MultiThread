# OS HW#3 작업 정리 문서

## 1. 과제 핵심 요약

HW#3의 목표는 **multi-threaded word search program**을 구현하는 것이다.

프로그램은 지정된 디렉토리와 모든 하위 디렉토리를 재귀적으로 탐색하면서 텍스트 파일을 찾고, 여러 worker thread가 각 파일에서 지정된 단어를 검색한 뒤 파일별 검색 개수와 전체 검색 개수를 출력해야 한다.

이 과제는 단순한 파일 검색 프로그램이 아니라, 다음 개념을 실제로 적용하는 과제이다.

- Command line argument 처리
- Multi-thread programming
- Synchronization tools 사용
- Directory and file processing
- Bounded buffer 기반 producer-consumer 구조
- Performance improvement 적용 및 비교

---

## 2. 프로그램 실행 조건

프로그램 이름은 반드시 `mtws`이다.

실행 형식은 다음과 같다.

```bash
./mtws -b <buffer size> -t <num threads> -d <directory> -w <word>
```

각 옵션의 의미는 다음과 같다.

| 옵션 | 의미 |
|---|---|
| `-b` | bounded buffer 크기 |
| `-t` | 단어를 검색할 worker thread 개수, main thread 제외 |
| `-d` | 검색할 디렉토리, 하위 디렉토리까지 재귀적으로 탐색 |
| `-w` | 검색할 단어, 대소문자 구분 없이 검색 |

예시 실행 명령어는 다음과 같다.

```bash
./mtws -b 5 -t 3 -d /home/dir1 -w hello
```

이 경우 bounded buffer 크기는 5이고, worker thread는 3개이며, `/home/dir1` 아래의 모든 하위 디렉토리를 탐색하면서 `hello`를 대소문자 구분 없이 검색한다.

---

## 3. 구현 우선순위

### 1순위: 명령어 인자 처리

`getopt()`와 `optarg`를 사용하여 다음 값을 받아야 한다.

- buffer size
- number of worker threads
- search directory
- search word

처리해야 할 내용은 다음과 같다.

- 옵션이 하나라도 빠지면 usage 출력
- buffer size가 0 이하이면 오류 처리
- thread 개수가 0 이하이면 오류 처리
- directory가 존재하지 않으면 오류 처리
- search word가 비어 있으면 오류 처리

---

### 2순위: bounded buffer 구조 설계

bounded buffer는 producer thread와 worker thread들이 공유하는 큐이다.

필요한 데이터는 다음과 같다.

```text
buffer
front
rear
count
capacity
mutex
not_empty condition variable
not_full condition variable
done flag
```

bounded buffer는 circular queue 방식으로 관리한다.

- `front`: worker thread가 꺼낼 위치
- `rear`: producer가 넣을 위치
- `count`: 현재 buffer 안에 들어 있는 파일 경로 개수
- `capacity`: `-b`로 받은 buffer 크기

---

### 3순위: producer 역할 구현

producer 역할을 하는 thread는 `-d`로 받은 디렉토리를 재귀적으로 탐색한다.

해야 할 일은 다음과 같다.

1. 검색 디렉토리를 연다.
2. 디렉토리 안의 항목을 하나씩 확인한다.
3. 항목이 디렉토리이면 재귀적으로 다시 탐색한다.
4. 항목이 일반 파일이면 파일 경로를 bounded buffer에 넣는다.
5. 모든 파일 탐색이 끝나면 `done = true`로 바꾸고 worker thread들을 깨운다.

의사코드는 다음과 같다.

```text
search_directory(path):
    open path

    for each entry in path:
        if entry is "." or "..":
            skip

        full_path = path + "/" + entry

        if full_path is directory:
            search_directory(full_path)

        else if full_path is regular file:
            buffer_push(full_path)

    close path
```

---

### 4순위: worker thread 생성

`-t` 옵션으로 받은 개수만큼 worker thread를 생성한다.

예를 들어 다음과 같이 실행하면:

```bash
./mtws -b 5 -t 3 -d ./dir1 -w hello
```

worker thread는 3개 생성되어야 한다.

각 worker thread는 다음 일을 반복한다.

1. bounded buffer에서 파일 경로를 하나 꺼낸다.
2. 꺼낸 파일에서 `-w` 단어를 검색한다.
3. 파일별 검색 결과를 출력한다.
4. local count에 검색 개수를 누적한다.
5. 더 이상 처리할 파일이 없으면 종료한다.

---

### 5순위: bounded buffer push 구현

producer는 파일 경로를 buffer에 넣는다.

buffer가 가득 차 있으면 producer는 기다려야 한다.

의사코드는 다음과 같다.

```text
buffer_push(file_path):
    lock mutex

    while buffer is full:
        wait not_full

    buffer[rear] = file_path
    rear = (rear + 1) % capacity
    count++

    signal not_empty

    unlock mutex
```

주의할 점은 `if`가 아니라 `while`을 사용해야 한다는 것이다. thread가 깨어났더라도 다른 thread가 먼저 buffer 상태를 바꿨을 수 있기 때문이다.

---

### 6순위: bounded buffer pop 구현

worker thread는 buffer에서 파일 경로를 꺼낸다.

buffer가 비어 있고 producer가 아직 탐색 중이면 worker는 기다려야 한다.

의사코드는 다음과 같다.

```text
buffer_pop():
    lock mutex

    while buffer is empty and done is false:
        wait not_empty

    if buffer is empty and done is true:
        unlock mutex
        return NULL

    file_path = buffer[front]
    front = (front + 1) % capacity
    count--

    signal not_full

    unlock mutex
    return file_path
```

`done == true`이고 buffer가 비어 있으면 더 이상 처리할 파일이 없다는 뜻이므로 worker thread는 종료한다.

---

### 7순위: 단어 검색 함수 구현

각 worker thread는 파일 하나를 열고, 지정된 단어가 몇 번 나오는지 세야 한다.

조건은 **case-insensitive**이다.

즉 다음 문자열은 같은 단어로 처리해야 한다.

```text
hello
Hello
HELLO
HeLLo
```

기본 흐름은 다음과 같다.

```text
search_word_in_file(file_path, search_word):
    count = 0

    open file

    while read line:
        convert line to lowercase
        find lowercase search_word in line
        increase count

    close file

    return count
```

검색 단어는 프로그램 시작 시 한 번만 lowercase로 바꿔두는 것이 좋다.

---

### 8순위: 전체 결과 합산

파일별 검색 결과를 출력하고, 마지막에는 전체 검색 개수를 출력해야 한다.

필요한 결과값은 다음과 같다.

```text
total_found
total_files
```

여러 worker thread가 동시에 `total_found`와 `total_files`를 수정하면 race condition이 발생할 수 있으므로 mutex가 필요하다.

다만 성능 개선을 위해 각 worker thread가 `local_found`, `local_files`를 먼저 누적하고, thread 종료 직전에 한 번만 global result에 더하는 방식을 추천한다.

기본 방식:

```text
파일 하나 처리할 때마다 global total에 lock
```

개선 방식:

```text
worker thread 내부에서 local total에 누적
worker 종료 직전에 한 번만 global total에 lock
```

---

### 9순위: 출력 형식 정리

출력은 PDF의 예시처럼 다음 내용을 포함해야 한다.

```text
Buffer size=10, Num threads=3, Directory=./dir1, SearchWord=printf
[Thread#2] started searching 'printf'...
[Thread#0] started searching 'printf'...
[Thread#2-0] ./dir1/fork2.c : 2 found
[Thread#1-7] ./dir1/fork1.c : 5 found
Total found = 53 (Num files=25)
```

thread scheduling 때문에 파일 처리 순서는 실행할 때마다 달라질 수 있다. 이것은 정상 동작이다.

---

## 4. 동기화 도구 사용 조건

bounded buffer에 접근할 때는 반드시 동기화가 필요하다.

사용할 수 있는 도구는 다음과 같다.

- POSIX mutex
- POSIX semaphore
- POSIX condition variable

추천 구조는 다음이다.

```text
pthread_mutex_t mutex
pthread_cond_t not_empty
pthread_cond_t not_full
```

사용 이유는 다음과 같다.

- producer와 worker thread가 동시에 buffer에 접근하면 race condition이 발생할 수 있다.
- buffer가 full이면 producer는 기다려야 한다.
- buffer가 empty이면 worker thread는 기다려야 한다.
- condition variable을 사용하면 busy waiting 없이 thread를 재울 수 있다.

---

## 5. 성능 개선 조건

Advanced requirement에서는 성능 개선 방법을 제안하고 적용해야 한다.

단, 다음 방식은 성능 개선으로 인정되지 않는다.

```text
thread 개수만 바꾸기
bounded buffer 크기만 바꾸기
```

추천 성능 개선 방법은 다음이다.

### 성능 개선 방법: thread-local aggregation

기존 방식은 파일 하나를 처리할 때마다 global total에 접근한다.

```text
lock result_mutex
total_found += file_count
total_files += 1
unlock result_mutex
```

이 방식은 파일 개수가 많을수록 mutex lock/unlock 횟수가 증가한다.

개선 방식은 각 worker thread가 자기 local 변수에 먼저 누적하고, 종료 직전에 한 번만 global total에 더하는 것이다.

```text
local_found += file_count
local_files += 1

worker 종료 직전:
    lock result_mutex
    total_found += local_found
    total_files += local_files
    unlock result_mutex
```

이 방식의 장점은 다음과 같다.

- result mutex에 접근하는 횟수가 줄어든다.
- worker thread 간 lock contention이 감소한다.
- 파일 수가 많을수록 개선 효과를 설명하기 쉽다.
- 코드 수정 범위가 명확해서 영상에서 설명하기 좋다.

---

## 6. 실험 조건

baseline version과 improved version을 비교해야 한다.

### Baseline version

- 파일 하나를 처리할 때마다 global total에 mutex lock
- 기본 bounded buffer 구조 사용
- 기본 worker thread 구조 사용

### Improved version

- worker thread별 local count 사용
- worker 종료 직전에 global total에 한 번만 반영
- result mutex lock 횟수 감소

### 실행 예시

```bash
make
time ./mtws -b 10 -t 3 -d ./dir1 -w printf
```

### 비교할 항목

| 항목 | Baseline | Improved |
|---|---:|---:|
| 실행 시간 | 측정값 입력 | 측정값 입력 |
| 총 파일 수 | 측정값 입력 | 측정값 입력 |
| 총 검색 개수 | 측정값 입력 | 측정값 입력 |
| result mutex 접근 횟수 | 파일 수만큼 | thread 수만큼 |

---

## 7. Makefile 조건

프로그램은 Ubuntu에서 Makefile로 컴파일되고 실행 가능해야 한다.

필요한 컴파일 옵션은 다음과 같다.

```bash
-Wall -pthread
```

Makefile은 다음 기능을 포함하는 것이 좋다.

```text
make
make clean
```

제출 전에 반드시 다음 명령어로 확인한다.

```bash
make clean
make
./mtws -b 10 -t 3 -d ./dir1 -w printf
```

---

## 8. 제출 파일 조건

제출 디렉토리에는 다음 파일이 포함되어야 한다.

```text
mtws.c
Makefile
AI usage report text file
recorded video file
```

추가 파일을 포함할 수 있으나, main source file 이름은 반드시 `mtws.c`여야 한다.

소스 코드는 다음 조건을 만족해야 한다.

- 적절한 주석 포함
- indentation 정리
- 함수 역할이 명확하게 분리됨
- Ubuntu에서 Makefile로 컴파일 가능
- 실행 결과가 과제 요구사항과 일치

---

## 9. 영상 제출 조건

영상 조건은 다음과 같다.

```text
길이: 7분 이하
파일 크기: 50MB 이하
얼굴이 보여야 함
한국어 또는 영어 설명 가능
```

영상에서 반드시 설명해야 하는 내용은 다음이다.

### 1. 구현 설명

- command line argument를 어떻게 처리했는지 설명
- main thread의 역할 설명
- producer 역할 설명
- worker thread의 역할 설명
- directory traversal 방식 설명
- word search 방식 설명

### 2. thread 동작 설명

- producer는 파일 경로를 찾아 bounded buffer에 넣음
- worker thread는 bounded buffer에서 파일 경로를 꺼냄
- worker thread는 파일별로 단어 개수를 검색함
- 모든 파일 처리가 끝나면 worker thread가 종료됨

### 3. synchronization tools 설명

- bounded buffer는 공유 자원이므로 mutex로 보호함
- buffer가 full이면 producer가 `not_full`에서 기다림
- buffer가 empty이면 worker가 `not_empty`에서 기다림
- `done` flag를 통해 producer 종료 여부를 worker에게 알려줌
- 전체 결과 변수는 result mutex로 보호함

### 4. 성능 개선 설명

- baseline에서는 파일 하나마다 global result에 접근함
- improved에서는 worker별 local result를 사용함
- worker 종료 시 한 번만 global result에 더함
- lock contention이 줄어 성능 개선을 기대할 수 있음

### 5. 실험 결과 설명

- baseline 실행 결과 보여주기
- improved 실행 결과 보여주기
- 전체 검색 결과가 같은지 확인하기
- 실행 시간이 어떻게 달라졌는지 비교하기

---

## 10. 영상 발표 대본 초안

아래 흐름대로 말하면 된다.

```text
안녕하세요. HW#3 multi-threaded word search program 구현 내용을 설명하겠습니다.

이 프로그램은 ./mtws -b buffer_size -t thread_count -d directory -w word 형식으로 실행됩니다.
-b는 bounded buffer 크기, -t는 worker thread 개수, -d는 검색 디렉토리, -w는 검색 단어입니다.

전체 구조는 producer-consumer 구조입니다.
main thread는 command line argument를 처리하고 worker thread를 생성합니다.
그 후 producer 역할을 하는 부분이 디렉토리를 재귀적으로 탐색하면서 파일 경로를 bounded buffer에 넣습니다.

worker thread들은 bounded buffer에서 파일 경로를 하나씩 꺼내고, 해당 파일에서 검색 단어가 몇 번 나오는지 계산합니다.
검색은 case-insensitive 방식으로 처리했습니다.

bounded buffer는 여러 thread가 동시에 접근하는 공유 자원이기 때문에 mutex와 condition variable을 사용했습니다.
buffer가 가득 차 있으면 producer는 not_full condition variable에서 기다리고,
buffer가 비어 있으면 worker thread는 not_empty condition variable에서 기다립니다.
producer가 모든 파일 탐색을 끝내면 done flag를 true로 바꾸고 기다리는 worker thread들을 깨웁니다.

성능 개선으로는 thread-local aggregation 방식을 적용했습니다.
baseline version에서는 파일 하나를 처리할 때마다 global total에 mutex lock을 걸었습니다.
improved version에서는 각 worker thread가 local_found에 결과를 누적하고, thread가 종료될 때 한 번만 global total에 더하도록 수정했습니다.
이를 통해 result mutex에 접근하는 횟수를 줄이고 lock contention을 줄일 수 있었습니다.

마지막으로 baseline version과 improved version을 같은 조건에서 실행하여 결과를 비교했습니다.
두 버전의 total found 값은 동일해야 하며, improved version에서는 mutex 접근 횟수가 줄어드는 것을 확인할 수 있습니다.
```

---

## 11. AI Usage Report 작성 예시

제출용 AI usage report에는 다음 내용을 포함해야 한다.

```text
I used AI tools to understand the requirements of HW#3 and to organize the implementation plan.
The AI helped me understand the producer-consumer structure, bounded buffer design, POSIX mutex, condition variables, and possible performance improvement strategies.

The prompts I entered included questions about how to structure the assignment, how bounded buffers work, and how to explain performance improvement in the recorded video.

I did not directly submit AI-generated code without understanding it.
I reviewed the suggested structure, implemented the program myself, tested it on Ubuntu, and verified that the output matched the assignment requirements.
I also modified the performance improvement part by applying thread-local aggregation and comparing the baseline and improved versions.
```

한국어로 작성하면 다음과 같다.

```text
본 과제에서 AI 도구를 사용하여 HW#3의 요구사항을 정리하고 구현 구조를 설계하는 데 도움을 받았다.
AI는 producer-consumer 구조, bounded buffer 설계, POSIX mutex, condition variable, 성능 개선 방법을 이해하는 데 도움을 주었다.

입력한 프롬프트는 과제 구조 정리, bounded buffer 동작 방식, 동기화 도구 사용 이유, 영상 설명 구성 등에 관한 질문이었다.

AI가 제안한 내용을 그대로 제출하지 않았고, 직접 코드를 작성하고 Ubuntu 환경에서 실행 결과를 확인하였다.
또한 thread-local aggregation 방식을 적용하여 baseline version과 improved version의 결과를 비교하고 검증하였다.
```

---

## 12. 최종 제출 전 체크리스트

### 코드 체크

- [ ] 프로그램 이름이 `mtws`로 컴파일되는가?
- [ ] main source file 이름이 `mtws.c`인가?
- [ ] `getopt()`로 `-b`, `-t`, `-d`, `-w`를 처리했는가?
- [ ] 잘못된 인자가 들어왔을 때 usage를 출력하는가?
- [ ] 디렉토리를 재귀적으로 탐색하는가?
- [ ] 파일 경로를 bounded buffer에 넣는가?
- [ ] worker thread가 buffer에서 파일 경로를 꺼내는가?
- [ ] 검색 단어를 case-insensitive로 처리하는가?
- [ ] 파일별 검색 결과를 출력하는가?
- [ ] 전체 검색 결과를 출력하는가?
- [ ] worker thread가 정상 종료되는가?
- [ ] mutex와 condition variable을 적절하게 사용하는가?
- [ ] race condition 가능성이 있는 공유 변수에 lock을 사용하는가?

### 성능 개선 체크

- [ ] 단순히 thread 개수나 buffer 크기만 바꾸지 않았는가?
- [ ] baseline version과 improved version을 구분했는가?
- [ ] 성능 개선 방법을 source code comment에 설명했는가?
- [ ] 영상에서 성능 개선 이유를 설명할 수 있는가?
- [ ] baseline과 improved 실행 결과를 비교했는가?

### 제출 체크

- [ ] `mtws.c` 포함
- [ ] `Makefile` 포함
- [ ] AI usage report 포함
- [ ] 7분 이하 영상 포함
- [ ] 영상 파일 크기 50MB 이하
- [ ] 얼굴이 보이게 녹화
- [ ] zip 파일 이름을 `hw3_studentid.zip` 형식으로 작성
- [ ] LMS에 제출

---

## 13. 최종 압축 파일 구조 예시

```text
hw3_학번.zip
├── mtws.c
├── Makefile
├── ai_usage_report.txt
└── hw3_video.mp4
```

---

## 14. 가장 중요한 주의사항

이 과제는 AI 사용이 허용되지만, AI가 제안한 내용이나 코드를 이해하지 못한 채 그대로 제출하면 안 된다.

반드시 다음을 지켜야 한다.

- 직접 이해한 코드만 제출한다.
- 직접 수정하고 테스트한다.
- 영상에서 본인이 구현 내용을 설명할 수 있어야 한다.
- AI usage report에 AI 사용 내용을 솔직하게 작성한다.
- Handong Honor Code를 준수한다.
