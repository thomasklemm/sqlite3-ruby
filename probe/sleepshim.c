// LD_PRELOAD shim: reports every nanosleep() that a signal cuts short, with
// the signal that interrupted it and the time left unslept.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static volatile sig_atomic_t last_signal;
static struct sigaction originals[65];

static void trampoline(int sig, siginfo_t *info, void *uctx) {
  last_signal = sig;
  struct sigaction *o = &originals[sig];
  if (o->sa_flags & SA_SIGINFO) {
    if (o->sa_sigaction) o->sa_sigaction(sig, info, uctx);
  } else if (o->sa_handler != SIG_DFL && o->sa_handler != SIG_IGN) {
    o->sa_handler(sig);
  }
}

int sigaction(int sig, const struct sigaction *act, struct sigaction *old) {
  static int (*real)(int, const struct sigaction *, struct sigaction *);
  if (!real) real = dlsym(RTLD_NEXT, "sigaction");
  if (!act || sig <= 0 || sig >= 65) return real(sig, act, old);
  int handled = (act->sa_flags & SA_SIGINFO) ? act->sa_sigaction != NULL
                                             : (act->sa_handler != SIG_DFL && act->sa_handler != SIG_IGN);
  if (!handled) return real(sig, act, old);
  struct sigaction prev = originals[sig];
  originals[sig] = *act;
  struct sigaction wrapped = *act;
  wrapped.sa_flags |= SA_SIGINFO;
  wrapped.sa_sigaction = trampoline;
  int rc = real(sig, &wrapped, old);
  if (old && rc == 0 && (old->sa_flags & SA_SIGINFO) && old->sa_sigaction == trampoline) *old = prev;
  return rc;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
  static int (*real)(const struct timespec *, struct timespec *);
  if (!real) real = dlsym(RTLD_NEXT, "nanosleep");
  struct timespec left;
  last_signal = 0;
  int rc = real(req, &left);
  if (rc == -1 && errno == EINTR) {
    fprintf(stderr, "[sleepshim] nanosleep(%ld.%09ld) interrupted by signal %d (%s), %ld.%09ld s unslept\n",
            (long)req->tv_sec, req->tv_nsec, last_signal, last_signal ? strsignal(last_signal) : "?",
            (long)left.tv_sec, left.tv_nsec);
    errno = EINTR;
  }
  if (rem) *rem = left;
  return rc;
}
