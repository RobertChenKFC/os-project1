#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <time.h>
#include <stdbool.h>
#include <limits.h>
#include <alloca.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#define SYS_MY_GETTIME 333
#define SYS_MY_PRINTTIME 334
#define ERROR {                                                                \
                fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__,             \
                                               strerror(errno));               \
                abort();                                                       \
              }
#define UNITS_OF_TIME(units) {                                                 \
                               for (volatile int i = 0; i < units; ++i)        \
                                 for (volatile unsigned long long i = 0;       \
                                      i < 1000000; ++i);                       \
                             }
#define SET_CPU(cpu) {                                                         \
                       cpu_set_t cpuSet;                                       \
                       CPU_ZERO(&cpuSet);                                      \
                       CPU_SET((cpu), &cpuSet);                                \
                       sched_setaffinity(0, sizeof(cpuSet), &cpuSet);          \
                     }
#define SET_PRIORITY(pid, policy, priority) {                                  \
                                              sched_param schedParam;          \
                                              schedParam.sched_priority =      \
                                                (priority);                    \
                                              if (sched_setscheduler(          \
                                                    (pid), (policy),           \
                                                    &schedParam) < 0)          \
                                                ERROR;                         \
                                            }
#define START_PROCESS(process) {                                               \
                                 if (write((process).toChild[1], "s", 1) != 1) \
                                   ERROR;                                      \
                               }
#define QUEUE_INIT(queue, n_) {                                                \
                               (queue).arr = alloca(sizeof(int) * (n_));       \
                               (queue).front = (queue).back = (n_);            \
                               (queue).n_ = (n);                               \
                             }
#define QUEUE_PUSH(queue, data) {                                              \
                                  (queue).arr[(queue).back] = data;            \
                                  (queue).back = ((queue).back + 1) %          \
                                                 (queue).n;                    \
                                }
#define QUEUE_POP(queue) ({                                                    \
                            int data = (queue).arr[(queue).front];             \
                            (queue).front = ((queue).front + 1) % (queue).n;   \
                            data;                                              \
                         })
#define QUEUE_FRONT(queue) ((queue).arr[(queue).front])

typedef struct {
  char name[32];
  int readyTime;
  int execTime;
  int index;
  int fromChild[2];
  int toChild[2];
  pid_t pid;
  bool started;
  bool finished;
} Process;

typedef struct {
  int* arr;
  int front;
  int back;
  int n;
} Queue;

typedef struct sched_param sched_param;
typedef struct timespec timespec;

void GetTime(timespec* t);
void PrintTime(pid_t pid, long startSec, long startNSec, long finishSec,
               long finishNSec);
int ProcessCmp(const void* pa, const void* pb);
void PolicyFIFO(Process* processes, int n);
void PolicyRR(Process* processes, int n);
void PolicySJF(Process* processes, int n);
void PolicyPSJF(Process* processes, int n);
pid_t ProcessInit(Process* process);

int main() {
#ifdef DEBUG
  setvbuf(stdout, NULL, _IONBF, 0);
#endif

  SET_CPU(0);

  char policy[8];
  if (scanf("%8s", policy) != 1) ERROR;
  
  int n;
  if (scanf("%d", &n) != 1) ERROR;
  Process* processes = calloc(n, sizeof(Process));
  if (!processes) ERROR;
  for (int i = 0; i < n; ++i) {
    if (scanf("%s%d%d", processes[i].name, &processes[i].readyTime,
                        &processes[i].execTime) != 3) ERROR;
    processes[i].index = i;
    if (pipe(processes[i].fromChild) == -1) ERROR;
    if (pipe(processes[i].toChild) == -1) ERROR;
    processes[i].pid = ProcessInit(&processes[i]);
  }

  qsort(processes, n, sizeof(Process), ProcessCmp);
  
#ifdef DEBUG
  printf("input:\n%s\n%d\n", policy, n);
  for (int i = 0; i < n; ++i) {
    printf("%s %d %d\n", processes[i].name, processes[i].readyTime,
                         processes[i].execTime);
  }
  printf("\n");
#endif

  for (int i = 0; i < n; ++i) {
    char c;
    if (read(processes[i].fromChild[0], &c, 1) != 1) ERROR;
  }

  if (strcmp(policy, "FIFO") == 0)
    PolicyFIFO(processes, n);
  else if (strcmp(policy, "RR") == 0)
    PolicyRR(processes, n);
  else if (strcmp(policy, "SJF") == 0)
    PolicySJF(processes, n);
  else if (strcmp(policy, "PSJF") == 0)
    PolicyPSJF(processes, n);

#ifdef DEBUG
  printf("spawned all processes, waiting\n");
  printf("main process running on cpu %d\n", sched_getcpu());
#endif

  pid_t childPid;
  while (childPid = wait(NULL)) {
    if (childPid < 0) {
      if (errno == ECHILD) break;
      ERROR;
    }
  }
}

void GetTime(timespec* t) {
  syscall(333, t);
}

void PrintTime(pid_t pid, long startSec, long startNSec, long finishSec,
               long finishNSec) {
  syscall(334, pid, startSec, startNSec, finishSec, finishNSec);
}

int ProcessCmp(const void* pa, const void* pb) {
  const Process* a = pa;
  const Process* b = pb;
  if (a->readyTime == b->readyTime)
    return a->index - b->index;
  return a->readyTime - b->readyTime;
}

void PolicyFIFO(Process* processes, int n) {
  int maxPriority = sched_get_priority_max(SCHED_FIFO);
  SET_PRIORITY(0, SCHED_FIFO, maxPriority);
  UNITS_OF_TIME(processes[0].readyTime);
  for (int i = 0; i < n; ++i) {
    SET_PRIORITY(processes[i].pid, SCHED_FIFO, maxPriority);
    START_PROCESS(processes[i]);
    if (waitpid(processes[i].pid, NULL, 0) < 0) ERROR;
  }
}

void PolicyRR(Process* processes, int n) {
  int maxPriority = sched_get_priority_max(SCHED_RR);
  SET_PRIORITY(0, SCHED_RR, maxPriority);
  UNITS_OF_TIME(processes[0].readyTime);
  int currentTime = processes[0].readyTime;
  int prevExecIndex = -1;
  int prevExecTime;
  Queue queue;
  QUEUE_INIT(queue, n);
  for (int i = 0, remainingProcesses = n; true;) {
    bool preempted = false;
    if (prevExecIndex != -1) {
      if (prevExecTime <= 500) {
  #ifdef DEBUG
        //printf("waiting for P%d to finish\n", prevExecIndex + 1);
  #endif
        if (waitpid(processes[prevExecIndex].pid, NULL, 0) < 0) ERROR;
        currentTime += prevExecTime;
        if (--remainingProcesses == 0)
          break;
        preempted = false;
      } else {
  #ifdef DEBUG
        //printf("executing 500 time units for P%d (execTime = %d)\n",
               //prevExecIndex + 1, prevExecTime);
  #endif
        UNITS_OF_TIME(500);
        currentTime += 500;
        processes[prevExecIndex].execTime -= 500;
        preempted = true;
      }
    }
    for (; i < n && processes[i].readyTime <= currentTime; ++i)
      QUEUE_PUSH(queue, i);
    if (preempted)
      QUEUE_PUSH(queue, prevExecIndex);
    int execIndex = QUEUE_POP(queue);
#ifdef DEBUG
    //printf("current time %d, chosen P%d\n", currentTime, execIndex + 1);
#endif
    if (execIndex != prevExecIndex) {
      if (preempted) {
#ifdef DEBUG
        printf("%d: P%d preempted (%d), P%d chosen (%d)\n", currentTime,
               prevExecIndex + 1, processes[prevExecIndex].execTime,
               execIndex + 1, processes[execIndex].execTime);
#endif
        SET_PRIORITY(processes[prevExecIndex].pid, SCHED_RR, maxPriority - 1);
      }
#ifdef DEBUG
      else {
        printf("%d: P%d finished, P%d chosen (%d)\n", currentTime,
               prevExecIndex + 1, execIndex + 1, processes[execIndex].execTime);
      }
#endif
      SET_PRIORITY(processes[execIndex].pid, SCHED_RR, maxPriority);
      if (!processes[execIndex].started) {
        START_PROCESS(processes[execIndex]);
        processes[execIndex].started = true;
#ifdef DEBUG
        //printf("P%d started\n", execIndex + 1);
#endif
      }
    }
    prevExecIndex = execIndex;
    prevExecTime = processes[execIndex].execTime;
  }
}

void PolicySJF(Process* processes, int n) {
  int maxPriority = sched_get_priority_max(SCHED_FIFO);
  SET_PRIORITY(0, SCHED_FIFO, maxPriority);
  UNITS_OF_TIME(processes[0].readyTime);
  int currentTime = processes[0].readyTime;
  for (int i = 0; i < n; ++i) {
    int execTime = INT_MAX;
    int execIndex = -1;
    for (int j = 0; j < n && processes[j].readyTime <= currentTime; ++j) {
#ifdef DEBUG
      printf("process[%d].readyTime = %d, process[%d].execTime = %d, "
             "currentTime = %d\n", j, processes[j].readyTime,
                                   j, processes[j].execTime, currentTime);
#endif
      if (!processes[j].started && processes[j].execTime < execTime) {
        execTime = processes[j].execTime;
        execIndex = j;
      }
    }
#ifdef DEBUG
    fprintf(stderr, "starting process P%d\n", execIndex + 1);
    if (i > n) abort();
#endif
    SET_PRIORITY(processes[execIndex].pid, SCHED_FIFO, maxPriority);
    START_PROCESS(processes[execIndex]);
    processes[execIndex].started = true;
    if (waitpid(processes[execIndex].pid, NULL, 0) < 0) ERROR;
    currentTime += execTime;
  }
}

void PolicyPSJF(Process* processes, int n) {
  int maxPriority = sched_get_priority_max(SCHED_RR);
  SET_PRIORITY(0, SCHED_RR, maxPriority);
  int currentTime = 0;
  int prevExecIndex = -1;
  int prevExecTime = INT_MAX;
  for (int i = 0, remainingProcesses = n; remainingProcesses > 0;) {
    int waitingTime;
    bool preempted = false;
    if (i == n ||
        ((waitingTime = processes[i].readyTime - currentTime) >= prevExecTime &&
        prevExecIndex != -1)) {
#ifdef DEBUG
      printf("i = %d, waitingTime = %d, "
             "prevExecTime = %d, waiting for %d\n", i, waitingTime,
                                                    prevExecTime,
                                                    processes[prevExecIndex].pid);
#endif
      waitingTime = prevExecTime;
      if (waitpid(processes[prevExecIndex].pid, NULL, 0) < 0) ERROR;
      processes[prevExecIndex].finished = true;
      --remainingProcesses;
    } else {
      UNITS_OF_TIME(waitingTime);
      preempted = true;
    }
    currentTime += waitingTime;
#ifdef DEBUG
    printf("currentTime = %d\n", currentTime);
#endif
    if (prevExecIndex != -1)
      processes[prevExecIndex].execTime -= waitingTime;
    int execTime = INT_MAX;
    int execIndex = -1;
    for (i = 0; i < n && processes[i].readyTime <= currentTime; ++i) {
      if (!processes[i].finished && processes[i].execTime < execTime) {
        execTime = processes[i].execTime;
        execIndex = i;
      }
    }
#ifdef DEBUG
    printf("current time = %d, process P%d chosen\n", currentTime,
           execIndex + 1);
#endif
    if (execIndex != prevExecIndex && execIndex != -1) {
      if (prevExecIndex != -1 && preempted) {
        SET_PRIORITY(processes[prevExecIndex].pid, SCHED_RR, maxPriority - 1);
#ifdef DEBUG
        printf("decrease priority of P%d (%d)\n", prevExecIndex + 1,
                                                  processes[prevExecIndex].pid);
#endif
      }
      SET_PRIORITY(processes[execIndex].pid, SCHED_RR, maxPriority);
#ifdef DEBUG
      printf("increase priority of P%d (%d)\n", execIndex + 1,
                                                processes[execIndex].pid);
#endif
      if (!processes[execIndex].started) {
        START_PROCESS(processes[execIndex]);
        processes[execIndex].started = true;
      }
    }
    prevExecIndex = execIndex;
    prevExecTime = execTime;
  }
}

pid_t ProcessInit(Process* process) {
  pid_t childPid = fork();
  if (childPid < 0) {
    ERROR;
  } else if (childPid == 0) {
    SET_CPU(1);
    if (write(process->fromChild[1], "r", 1) != 1) ERROR;

#ifdef DEBUG
    printf("P%d (%d) waiting for parent\n", process->index + 1, getpid());
#endif
    char c;
    if (read(process->toChild[0], &c, 1) != 1) ERROR;
#ifdef DEBUG
    printf("P%d (%d) resume\n", process->index + 1, getpid());
#endif

    timespec startTime;
    GetTime(&startTime);

    childPid = getpid();
    printf("%s %d\n", process->name, childPid);
    UNITS_OF_TIME(process->execTime);

    timespec finishTime;
    GetTime(&finishTime);
    PrintTime(childPid, startTime.tv_sec, startTime.tv_nsec, finishTime.tv_sec,
              finishTime.tv_nsec);

#ifdef DEBUG
  printf("child process %d running on cpu %d\n", childPid, sched_getcpu());
#endif

    exit(0);
  }
  return childPid;
}

