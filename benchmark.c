#include <omp.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define n 100
#define barrier() (__sync_synchronize())

#include <sys/time.h>
double getCurrentTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int sum = 0;
int test() {
  sum += 1;
  // printf("sum ++, now is %d\n", sum);
}

typedef struct peterson_mutex_t {
  int level[n];
  int waiting[n - 1];
} peterson_mutex_t;

int peterson_mutex_init(peterson_mutex_t* mutex) {
  memset(mutex->level, -1, n * sizeof(int));
  memset(mutex->waiting, -1, n * sizeof(int));
}

int is_highest_level(peterson_mutex_t* mutex, int i, int l) {
  for (int j = 0; j < n; ++j) {
    if (j == i) {
      continue;
    }
    if (mutex->level[j] >= l) {
      return 0;
    }
  }
  return 1;
}

int peterson_mutex_lock(peterson_mutex_t* mutex, int i) {
  for (int l = 0; l < n - 1; ++l) {
    mutex->level[i] = l;
    barrier();
    mutex->waiting[l] = i;
    while (mutex->waiting[l] == i && !is_highest_level(mutex, i, l)) {
      printf("peterson %d wait lock\n", i);
    }
  }
}

int peterson_mutex_unlock(peterson_mutex_t* mutex, int i) {
  mutex->level[i] = -1;
}

peterson_mutex_t peterson_mutex;

void* peterson(void* arg) {
  int i = *(int*)arg;
  sleep(1);
  peterson_mutex_lock(&peterson_mutex, i);
  test();
  peterson_mutex_unlock(&peterson_mutex, i);
}

double peterson_main() {
  peterson_mutex_init(&peterson_mutex);

  pthread_t tid_list[n];
  int index[n] = {0};

  double start = omp_get_wtime();
  for (int i = 0; i < n; ++i) {
    index[i] = i;
    pthread_t tid;
    pthread_create(&tid, NULL, peterson, index + i);
    tid_list[i] = tid;
  }
  for (int i = 0; i < n; ++i) {
    pthread_join(tid_list[i], NULL);
  }
  double end = omp_get_wtime();
  printf("peterson_sum %d\n", sum);
  return end - start;
}

pthread_mutex_t pthread_mutx = PTHREAD_MUTEX_INITIALIZER;

// cmpxchgl
void* pthread_mutex(void* arg) {
  int i = *(int*)arg;
  sleep(1);
  int err = pthread_mutex_trylock(&pthread_mutx);
  if (err != 0) {
    printf("pthread wait lock\n");
    pthread_mutex_lock(&pthread_mutx);
  }

  test();
  pthread_mutex_unlock(&pthread_mutx);
}

double pthread_mutex_main() {
  pthread_t tid_list[n];
  int index[n] = {0};
  double start = omp_get_wtime();
  for (int i = 0; i < n; ++i) {
    index[i] = i;
    pthread_t tid;
    pthread_create(&tid, NULL, pthread_mutex, index + i);
    tid_list[i] = tid;
  }
  for (int i = 0; i < n; ++i) {
    pthread_join(tid_list[i], NULL);
  }
  double end = omp_get_wtime();
  printf("pthread_sum %d\n", sum);
  return end - start;
}

typedef struct bakery_mutex_t {
  int choosing[n];
  int number[n];
} bakery_mutex_t;

int bakery_mutex_init(bakery_mutex_t* mutex) {
  memset(mutex->choosing, 0, n * sizeof(int));
  memset(mutex->number, 0, n * sizeof(int));
}

bakery_mutex_t bakery_mutex;

int max(int* arr, int size) {
  int max = -1;
  for (int i = 0; i < size; ++i) {
    if (arr[i] > max) {
      max = arr[i];
    }
  }
  return max;
}

int less_than(int a1, int a2, int b1, int b2) {
  if (a1 == b1) {
    return a2 < b2;
  } else
    return a1 < b1;
}

int bakery_mutex_lock(bakery_mutex_t* mutex, int i) {
  mutex->choosing[i] = 1;
  mutex->number[i] = max(mutex->number, n) + 1;
  mutex->choosing[i] = 0;
  for (int j = 0; j < n; ++j) {
    while (mutex->choosing[j])
      ;
    while (mutex->number[j] != 0 &&
           less_than(mutex->number[j], j, mutex->number[i], i)) {
      printf("bakery %d wait lock\n", i);
    }
  }
}

int bakery_mutex_unlock(bakery_mutex_t* mutex, int i) {
  mutex->number[i] = 0;
}

void* bakery(void* arg) {
  int i = *(int*)arg;
  sleep(1);
  bakery_mutex_lock(&bakery_mutex, i);
  test();
  bakery_mutex_unlock(&bakery_mutex, i);
}

double bakery_main() {
  bakery_mutex_init(&bakery_mutex);

  pthread_t tid_list[n] = {0};
  double start = omp_get_wtime();

  int index[n] = {0};

  for (int i = 0; i < n; ++i) {
    index[i] = i;
    pthread_t tid;
    pthread_create(&tid, NULL, bakery, index + i);
    tid_list[i] = tid;
  }
  for (int i = 0; i < n; ++i) {
    pthread_join(tid_list[i], NULL);
  }
  double end = omp_get_wtime();

  printf("bakery_sum %d\n", sum);
  return end - start;
}

/* McGuire */
enum pstate { IDLE, WAITING, ACTIVE };

typedef struct McGuire_mutex_t {
  enum pstate flags[n];
  int turn;
} McGuire_mutex_t;

int McGuire_mutex_init(McGuire_mutex_t* mutex) {
  memset(mutex->flags, IDLE, n * sizeof(enum pstate));
  mutex->turn = 0;
}

int McGuire_mutex_lock(McGuire_mutex_t* mutex, int i) {
  int idx = 0;
  do {
    // sleep(1); //此处的sleep必会竞态,但不公平
    mutex->flags[i] = WAITING;
    idx = mutex->turn;
    while (idx != i) {
      if (mutex->flags[idx] != IDLE) {
        idx = mutex->turn;
        printf("McGuire %d wait lock\n", i);
      } else
        idx = (idx + 1) % n;
    }
    mutex->flags[i] = ACTIVE;
    idx = 0;
    while (idx < n && (idx == i || mutex->flags[idx] != ACTIVE)) {
      idx = idx + 1;
    }
  } while (idx < n || (mutex->turn == i && mutex->flags[mutex->turn] == IDLE));
  mutex->turn = i;
}

int McGuire_mutex_unlock(McGuire_mutex_t* mutex, int i) {
  int idx = (mutex->turn + 1) % n;
  while (mutex->flags[idx] == IDLE) {
    idx = (idx + 1) % n;
  }
  mutex->turn = idx;
  mutex->flags[i] = IDLE;
}

McGuire_mutex_t MG_mutex;

void* McGuire(void* arg) {
  int i = *(int*)arg;
  sleep(1);
  McGuire_mutex_lock(&MG_mutex, i);
  test();
  McGuire_mutex_unlock(&MG_mutex, i);
}

double McGuire_main() {
  McGuire_mutex_init(&MG_mutex);

  pthread_t tid_list[n] = {0};
  double start = omp_get_wtime();

  int index[n] = {0};
  memset(index, 0, n * sizeof(int));
  for (int i = 0; i < n; ++i) {
    index[i] = i;
    pthread_t tid;
    pthread_create(&tid, NULL, McGuire, index + i);
    tid_list[i] = tid;
  }
  for (int i = 0; i < n; ++i) {
    pthread_join(tid_list[i], NULL);
  }
  double end = omp_get_wtime();
  printf("McGuire_sum %d\n", sum);
  return end - start;
}

int main() {
  double dur_sum1 = 0;
  double dur_sum2 = 0;
  double dur_sum3 = 0;
  double dur_sum4 = 0;
  const int N = 3;
  for (int i = 0; i < N; ++i) {
    double dur1 = peterson_main();
    // double dur1 = 0;
    double dur2 = pthread_mutex_main();
    // double dur2 = 0;
    double dur3 = bakery_main();
    // double dur3 = 0;
    double dur4 = McGuire_main();
    printf(
        "loop %d - %d processes | peterson: %lf , pthread_mutex: %lf , "
        "lamport_bakery: %lf , McGuire: %lf\n",
        i + 1, n, dur1, dur2, dur3, dur4);
    dur_sum1 += dur1;
    dur_sum2 += dur2;
    dur_sum3 += dur3;
    dur_sum4 += dur4;
    sum = 0;
  }
  printf(
      "Average delay in %d times with %d processes |||  peterson: %lf , "
      "pthread_mutex: %lf , "
      "lamport_bakery: "
      "%lf, McGuire: %lf\n",
      N, n, dur_sum1 / N, dur_sum2 / N, dur_sum3 / N, dur_sum4 / N);
}
