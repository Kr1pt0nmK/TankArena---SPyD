#ifndef THREAD_H
#define THREAD_H

/* Capa de abstraccion de hilos y exclusion mutua: oculta Win32 threads vs
   pthreads, y CRITICAL_SECTION vs pthread_mutex_t, tras una sola interfaz. */

#ifdef _WIN32
  #include <windows.h>
  typedef HANDLE            thread_t;
  typedef CRITICAL_SECTION  mutex_t;
#else
  #include <pthread.h>
  typedef pthread_t         thread_t;
  typedef pthread_mutex_t   mutex_t;
#endif

typedef void *(*thread_fn)(void *);

int  thread_create(thread_t *t, thread_fn fn, void *arg);  /* 0 = ok */
void thread_join(thread_t t);

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);
void mutex_destroy(mutex_t *m);

void sleep_ms(int ms);

#endif /* THREAD_H */
