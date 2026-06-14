#include "thread.h"
#include <stdlib.h>

#ifdef _WIN32

#include <process.h>

struct tramp { thread_fn fn; void *arg; };

static unsigned __stdcall win_tramp(void *p)
{
    struct tramp t = *(struct tramp *)p;
    free(p);
    t.fn(t.arg);
    return 0;
}

int thread_create(thread_t *t, thread_fn fn, void *arg)
{
    struct tramp *tr = (struct tramp *)malloc(sizeof(*tr));
    if (!tr) return -1;
    tr->fn = fn; tr->arg = arg;
    uintptr_t h = _beginthreadex(NULL, 0, win_tramp, tr, 0, NULL);
    if (!h) { free(tr); return -1; }
    *t = (HANDLE)h;
    return 0;
}

void thread_join(thread_t t) { WaitForSingleObject(t, INFINITE); CloseHandle(t); }

void mutex_init(mutex_t *m)    { InitializeCriticalSection(m); }
void mutex_lock(mutex_t *m)    { EnterCriticalSection(m); }
void mutex_unlock(mutex_t *m)  { LeaveCriticalSection(m); }
void mutex_destroy(mutex_t *m) { DeleteCriticalSection(m); }

void sleep_ms(int ms) { Sleep((DWORD)ms); }

#else  /* POSIX */

#include <time.h>

int thread_create(thread_t *t, thread_fn fn, void *arg) { return pthread_create(t, NULL, fn, arg); }
void thread_join(thread_t t)   { pthread_join(t, NULL); }

void mutex_init(mutex_t *m)    { pthread_mutex_init(m, NULL); }
void mutex_lock(mutex_t *m)    { pthread_mutex_lock(m); }
void mutex_unlock(mutex_t *m)  { pthread_mutex_unlock(m); }
void mutex_destroy(mutex_t *m) { pthread_mutex_destroy(m); }

void sleep_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

#endif
