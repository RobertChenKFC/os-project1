#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
  char name[32];
  int readyTime;
  int execTime;
  int index;
  pid_t pid;
  bool started;
  bool finished;
  int startTime;
  int finishTime;
} Process;

typedef struct {
  int* queue;
  int n;
  int front;
  int back;
} Queue;

Queue* QueueNew(int n);
void QueueDelete(Queue* queue);
void QueuePush(Queue* queue, int data);
int QueuePop(Queue* queue);
int ProcessCmp(const void* pa, const void* pb);
void PolicyFIFO(Process* processes, int n);
void PolicyRR(Process* processes, int n);
void PolicySJF(Process* processes, int n);
void PolicyPSJF(Process* process, int n);

int main() {
  setvbuf(stdout, NULL, _IONBF, 0);

  char policy[8];
  if (scanf("%8s", policy) != 1) abort();
  
  int n;
  if (scanf("%d", &n) != 1) abort();
  Process* processes = calloc(n, sizeof(Process));
  for (int i = 0; i < n; ++i) {
    if (scanf("%s%d%d", processes[i].name, &processes[i].readyTime,
                        &processes[i].execTime) != 3) abort();
    processes[i].index = i;
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

  if (strcmp(policy, "FIFO") == 0)
    PolicyFIFO(processes, n);
  else if (strcmp(policy, "RR") == 0)
    PolicyRR(processes, n);
  else if (strcmp(policy, "SJF") == 0)
    PolicySJF(processes, n);
  else if (strcmp(policy, "PSJF") == 0)
    PolicyPSJF(processes, n);
  
  printf("absolute:\n");
  for (int i = 0; i < n; ++i)
    printf("%s: (%d, %d)\n", processes[i].name, processes[i].startTime,
                             processes[i].finishTime);
  int minTime = INT_MAX;
  for (int i = 0; i < n; ++i) {
    if (processes[i].startTime < minTime)
      minTime = processes[i].startTime;
  }
  printf("relative:\n");
  for (int i = 0; i < n; ++i)
    printf("%s: (%d, %d)\n", processes[i].name,
                             processes[i].startTime - minTime,
                             processes[i].finishTime - minTime);

  free(processes);
}

Queue* QueueNew(int n) {
  Queue* queue = malloc(sizeof(Queue));
  queue->queue = malloc(sizeof(int) * n);
  queue->n = n;
  queue->front = queue->back = 0;
  return queue;
}

void QueueDelete(Queue* queue) {
  free(queue->queue);
  free(queue);
}

void QueuePush(Queue* queue, int data) {
  queue->queue[queue->back] = data;
  queue->back = (queue->back + 1) % queue->n;
}

int QueuePop(Queue* queue) {
  int data = queue->queue[queue->front];
  queue->front = (queue->front + 1) % queue->n;
  return data;
}

int ProcessCmp(const void* pa, const void* pb) {
  const Process* a = pa;
  const Process* b = pb;
  if (a->readyTime == b->readyTime)
    return a->index - b->index;
  return a->readyTime - b->readyTime;
}

void PolicyFIFO(Process* processes, int n) {
  int currentTime = processes[0].startTime;
  for (int i = 0; i < n; ++i) {
    processes[i].startTime = currentTime;
    processes[i].finishTime = (currentTime += processes[i].execTime);
  }
}

void PolicyRR(Process* processes, int n) {
  Queue* queue = QueueNew(n);

  int lastAddedIndex = -1;
  int execIndex = -1;
  int execTime = 0;
  int finishedProcesses = 0;
  for (int currentTime = processes[0].readyTime; finishedProcesses < n;
       ++currentTime) {
    for (int i = lastAddedIndex + 1; i < n &&
         processes[i].readyTime <= currentTime; ++i) {
      QueuePush(queue, lastAddedIndex = i);
#ifdef DEBUG
      // printf("%d: added P%d to queue\n", currentTime, i + 1);
#endif
    }
    if (execIndex == -1) {
      execIndex = QueuePop(queue);
#ifdef DEBUG
      //printf("%d: initial chosen process P%d (%d)\n", currentTime,
             //execIndex + 1, processes[execIndex].execTime);
#endif
    } else if (processes[execIndex].execTime == 0) {
#ifdef DEBUG
      int finishedIndex = execIndex;
#endif
      processes[execIndex].finishTime = currentTime;
      ++finishedProcesses;
      execIndex = QueuePop(queue);
#ifdef DEBUG
      printf("%d: P%d finished, P%d chosen (%d)\n", currentTime,
             finishedIndex + 1, execIndex + 1, processes[execIndex].execTime);
#endif
      execTime = 0;
    } else if (execTime == 500) {
#ifdef DEBUG
      int preemptedIndex = execIndex;
#endif
      QueuePush(queue, execIndex);
      execIndex = QueuePop(queue);
#ifdef DEBUG
      printf("%d: P%d prempted (%d), P%d chosen (%d)\n", currentTime,
             preemptedIndex + 1, processes[preemptedIndex].execTime,
             execIndex + 1, processes[execIndex].execTime);
#endif
      execTime = 0;
    }
    if (!processes[execIndex].started) {
#ifdef DEBUG
      //printf("P%d started (%d)\n", execIndex + 1,
             //processes[execIndex].execTime);
#endif
      processes[execIndex].started = true;
      printf("%s\n", processes[execIndex].name);
      processes[execIndex].startTime = currentTime;
    }
    ++execTime;
    --processes[execIndex].execTime;
  }

  QueueDelete(queue);
}

void PolicySJF(Process* processes, int n) {
  int currentTime = processes[0].readyTime;
  for (int i = 0; i < n; ++i) {
    int execTime = INT_MAX;
    int execIndex;
    for (int j = 0; j < n && processes[j].readyTime <= currentTime; ++j) {
      if (!processes[j].started && processes[j].execTime < execTime) {
        execTime = processes[j].execTime;
        execIndex = j;
      }
    }
#ifdef DEBUG
    printf("current time: %d, chosen %s\n", currentTime,
           processes[execIndex].name);
#endif
    processes[execIndex].started = true;
    processes[execIndex].startTime = currentTime;
    processes[execIndex].finishTime = (currentTime += execTime);
  }
}

void PolicyPSJF(Process* processes, int n) {
  int currentTime = processes[0].readyTime;
  int j;
  for (int remainingProcesses = n; remainingProcesses > 0;) {
    int execTime = INT_MAX;
    int execIndex;
    for (j = 0; j < n && processes[j].readyTime <= currentTime; ++j) {
      if (!processes[j].finished && processes[j].execTime < execTime) {
        execTime = processes[j].execTime;
        execIndex = j;
      }
    }
#ifdef DEBUG
    printf("current time %d, chosen %s\n", currentTime,
           processes[execIndex].name);
#endif
    if (!processes[execIndex].started) {
      processes[execIndex].started = true;
      processes[execIndex].startTime = currentTime;
#ifdef DEBUG
    printf("%s started\n", processes[execIndex].name);
#endif
    }
    int waitingTime = processes[j].readyTime - currentTime;
    if (j == n || execTime <= waitingTime) {
      processes[execIndex].finishTime = (currentTime += execTime);
      processes[execIndex].finished = true;
      --remainingProcesses;
#ifdef DEBUG
    printf("%s finished\n", processes[execIndex].name);
#endif
    } else {
      processes[execIndex].execTime -= waitingTime;
      currentTime = processes[j].readyTime;
#ifdef DEBUG
    printf("%s preempted with %d time left\n", processes[execIndex].name,
           processes[execIndex].execTime);
#endif
    }
  }
}

