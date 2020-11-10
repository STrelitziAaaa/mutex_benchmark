#include <omp.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#define N_THREAD 10
#define N_LOOP 100000

#define barrier() (__sync_synchronize())

double getCurrentTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int simple_add(int* sum) {
  (*sum)++;  // attention: ++ is prior to *
}

int atomic_add(int* sum) {
  __sync_fetch_and_add(sum, 1);
}

typedef struct arg {
  int i;  // the i-th thread
  int* sum;
} arg_t;

typedef struct peterson_mutex_t {
  int level[N_THREAD];
  int waiting[N_THREAD - 1];
} peterson_mutex_t;

int peterson_mutex_state[N_THREAD] = {0};
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
void print_mutex_state(int* states) {
  pthread_mutex_lock(&print_mutex);
  for (int i = 0; i < N_THREAD; i++) {
    printf(",%d" + !i, states[i]);
  }
  printf("\n");
  pthread_mutex_unlock(&print_mutex);
}

int peterson_mutex_init(peterson_mutex_t* mutex) {
  memset(mutex->level, -1, N_THREAD * sizeof(int));
  memset(mutex->waiting, -1, N_THREAD * sizeof(int));
}

int is_highest_level(peterson_mutex_t* mutex, int i, int l) {
  for (int j = 0; j < N_THREAD; ++j) {
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
  for (int l = 0; l < N_THREAD - 1; ++l) {
    mutex->level[i] = l;
    mutex->waiting[l] = i;
    while (mutex->waiting[l] == i && !is_highest_level(mutex, i, l)) {
    }
  }
  peterson_mutex_state[i] = 1;
}

int peterson_mutex_unlock(peterson_mutex_t* mutex, int i) {
  peterson_mutex_state[i] = 0;
  mutex->level[i] = -1;
}

// sem_t sem_peterson;

pthread_barrier_t barrier_peterson;
pthread_barrier_t barrier_pthreadM;
pthread_barrier_t barrier_bakery;
pthread_barrier_t barrier_mcguire;
pthread_barrier_t barrier_atomic;

peterson_mutex_t peterson_mutex;

void* peterson(void* arg) {
  arg_t* arg_ = (arg_t*)(arg);
  // printf("im wait:%d\n", arg_->i);
  // sem_wait(&sem_peterson);
  pthread_barrier_wait(&barrier_peterson);
  // printf("im run:%d\n", arg_->i);
  for (int i = 0; i < N_LOOP; i++) {
    peterson_mutex_lock(&peterson_mutex, arg_->i);
    simple_add(arg_->sum);
    peterson_mutex_unlock(&peterson_mutex, arg_->i);
  }
}

double peterson_main() {
  peterson_mutex_init(&peterson_mutex);
  // sem_init(&sem_peterson, 0, 0);
  pthread_barrier_init(&barrier_peterson, NULL, N_THREAD);
  pthread_t tid_list[N_THREAD];
  arg_t args[N_THREAD];
  int sum = 0;
  double start = omp_get_wtime();
  for (int i = 0; i < N_THREAD; ++i) {
    pthread_t tid;
    args[i].i = i;
    args[i].sum = &sum;
    pthread_create(&tid, NULL, peterson, &args[i]);
    tid_list[i] = tid;
  }
  // for (int i = 0; i < N_THREAD; ++i) {
  //   printf("sem_post:%d\n", i);
  //   sem_post(&sem_peterson);
  // }
  for (int i = 0; i < N_THREAD; ++i) {
    pthread_join(tid_list[i], NULL);
  }
  double end = omp_get_wtime();
  printf("peterson_sum %d\n", sum);
  return end - start;
}

pthread_mutex_t pthread_mutx;

// cmpxchgl
void* pthread_mutex(void* arg) {
  arg_t* arg_ = (arg_t*)(arg);
  pthread_barrier_wait(&barrier_pthreadM);
  for (int i = 0; i < N_LOOP; i++) {
    pthread_mutex_lock(&pthread_mutx);
    simple_add(arg_->sum);
    pthread_mutex_unlock(&pthread_mutx);
  }
}

double pthread_mutex_main() {
  pthread_mutex_init(&pthread_mutx, NULL);
  pthread_barrier_init(&barrier_pthreadM, NULL, N_THREAD);
  pthread_t tid_list[N_THREAD];
  arg_t args[N_THREAD];
  int sum = 0;
  double start = omp_get_wtime();
  for (int i = 0; i < N_THREAD; ++i) {
    pthread_t tid;
    args[i].i = i;
    args[i].sum = &sum;
    pthread_create(&tid, NULL, pthread_mutex, &args[i]);
    tid_list[i] = tid;
  }
  for (int i = 0; i < N_THREAD; ++i) {
    pthread_join(tid_list[i], NULL);
  }
  double end = omp_get_wtime();
  printf("pthread_sum %d\n", sum);
  return end - start;
}

typedef struct bakery_mutex_t {
  int choosing[N_THREAD];
  int number[N_THREAD];
} bakery_mutex_t;

int bakery_mutex_init(bakery_mutex_t* mutex) {
  memset(mutex->choosing, 0, N_THREAD * sizeof(int));
  memset(mutex->number, 0, N_THREAD * sizeof(int));
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
  // 可能读到相同的number,所以后面的less_than规定了如果相同则用线程序号break
  mutex->number[i] = max(mutex->number, N_THREAD) + 1;
  mutex->choosing[i] = 0;
  for (int j = 0; j < N_THREAD; ++j) {
    while (mutex->choosing[j])
      ;
    while ((mutex->number[j] != 0) &&
           less_than(mutex->number[j], j, mutex->number[i], i))
      ;
  }
}

int bakery_mutex_unlock(bakery_mutex_t* mutex, int i) {
  mutex->number[i] = 0;
}

void* bakery(void* arg) {
  arg_t* arg_ = (arg_t*)(arg);
  pthread_barrier_wait(&barrier_bakery);
  for (int i = 0; i < N_LOOP; i++) {
    bakery_mutex_lock(&bakery_mutex, arg_->i);
    simple_add(arg_->sum);
    bakery_mutex_unlock(&bakery_mutex, arg_->i);
  }
}

double bakery_main() {
  bakery_mutex_init(&bakery_mutex);
  pthread_barrier_init(&barrier_bakery, NULL, N_THREAD);

  pthread_t tid_list[N_THREAD] = {0};
  double start = omp_get_wtime();

  arg_t args[N_THREAD];
  int sum = 0;

  for (int i = 0; i < N_THREAD; ++i) {
    pthread_t tid;
    args[i].i = i;
    args[i].sum = &sum;
    pthread_create(&tid, NULL, bakery, &args[i]);
    tid_list[i] = tid;
  }
  for (int i = 0; i < N_THREAD; ++i) {
    pthread_join(tid_list[i], NULL);
  }
  double end = omp_get_wtime();

  printf("bakery_sum %d\n", sum);
  return end - start;
}

/* McGuire */
enum pstate { IDLE, WAITING, ACTIVE };

typedef struct McGuire_mutex_t {
  enum pstate flags[N_THREAD];
  int turn;
} McGuire_mutex_t;

int McGuire_mutex_init(McGuire_mutex_t* mutex) {
  memset(mutex->flags, IDLE, N_THREAD * sizeof(enum pstate));
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
      } else
        idx = (idx + 1) % N_THREAD;
    }
    mutex->flags[i] = ACTIVE;
    idx = 0;
    while (idx < N_THREAD && (idx == i || mutex->flags[idx] != ACTIVE)) {
      idx = idx + 1;
    }
  } while (idx < N_THREAD ||
           (mutex->turn == i && mutex->flags[mutex->turn] == IDLE));
  mutex->turn = i;
}

int McGuire_mutex_unlock(McGuire_mutex_t* mutex, int i) {
  int idx = (mutex->turn + 1) % N_THREAD;
  while (mutex->flags[idx] == IDLE) {
    idx = (idx + 1) % N_THREAD;
  }
  mutex->turn = idx;
  mutex->flags[i] = IDLE;
}

McGuire_mutex_t MG_mutex;

void* McGuire(void* arg) {
  arg_t* arg_ = (arg_t*)(arg);
  pthread_barrier_wait(&barrier_mcguire);
  for (int i = 0; i < N_LOOP; i++) {
    McGuire_mutex_lock(&MG_mutex, arg_->i);
    simple_add(arg_->sum);
    McGuire_mutex_unlock(&MG_mutex, arg_->i);
  }
}

double McGuire_main() {
  McGuire_mutex_init(&MG_mutex);
  pthread_barrier_init(&barrier_mcguire, NULL, N_THREAD);

  pthread_t tid_list[N_THREAD] = {0};
  double start = omp_get_wtime();

  arg_t args[N_THREAD];
  int sum = 0;

  for (int i = 0; i < N_THREAD; ++i) {
    args[i].i = i;
    args[i].sum = &sum;
    pthread_t tid;
    pthread_create(&tid, NULL, McGuire, &args[i]);
    tid_list[i] = tid;
  }
  for (int i = 0; i < N_THREAD; ++i) {
    pthread_join(tid_list[i], NULL);
  }
  double end = omp_get_wtime();
  printf("McGuire_sum %d\n", sum);
  return end - start;
}

void* atomic(void* arg) {
  arg_t* arg_ = (arg_t*)(arg);
  pthread_barrier_wait(&barrier_atomic);
  for (int i = 0; i < N_LOOP; i++) {
    atomic_add(arg_->sum);
  }
}

double atomic_main() {
  pthread_barrier_init(&barrier_atomic, NULL, N_THREAD);

  pthread_t tid_list[N_THREAD] = {0};
  double start = omp_get_wtime();

  arg_t args[N_THREAD];
  int sum = 0;

  for (int i = 0; i < N_THREAD; ++i) {
    args[i].i = i;
    args[i].sum = &sum;
    pthread_t tid;
    pthread_create(&tid, NULL, atomic, &args[i]);
    tid_list[i] = tid;
  }
  for (int i = 0; i < N_THREAD; ++i) {
    pthread_join(tid_list[i], NULL);
  }
  double end = omp_get_wtime();
  printf("atomic_sum %d\n", sum);
  return end - start;
}

int main() {
  printf("time:%f s\n----\n", peterson_main());
  printf("time:%f s\n----\n", pthread_mutex_main());
  printf("time:%f s\n----\n", bakery_main());
  printf("time:%f s\n----\n", McGuire_main());
  printf("time:%f s\n----\n", atomic_main());
}