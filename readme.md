# Benchmark
## 多进程软件锁
1. Peterson算法(filter算法)
2. lamport_bakery算法
3. McGuire算法
## 硬件锁: 
1. pthread_mutex


---
## 代码:
  - 以peterson为例,其他类同
  ```c
  peterson_mutex_t peterson_mutex;

  void* peterson(void* arg) {
    int i = *(int*)arg;
    // sleep(1);
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
  ```
---
## 结果:
- 说明: 在默认测试状态下,一般只有peterson会产生竞态问题!
```java
Average delay in 3 times with 200 processes |||  peterson: 0.547366 , pthread_mutex: 0.019146 , lamport_bakery: 0.019223, McGuire: 0.018633
```
---
## 为了使进程陷入竞态,暂且使用sleep(1)
- 由于软件锁都是忙等待模式,所以200进程的占用cpu忙等待有时候会卡死电脑,这里使用100进程
```java
Average delay in 3 times with 100 processes |||  peterson: 1.035813 , pthread_mutex: 1.016462 , lamport_bakery: 1.640425, McGuire: 1.936620
```
- 应改成用`alarm()`统一唤醒
---

---
- 硬件锁pthread_mutex_lock,使用`cmpxchgl`保证原子性,阻塞而不是自旋
- 可以尝试使用`__sync_fetch_and_and`
---
- 经测试,发现bakery算法存在问题,但是懒得改了.
