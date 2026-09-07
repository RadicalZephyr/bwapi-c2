/* R12: frame-handoff jitter -- game->bot->game round trip under five wait
   strategies, idle and contended. The tail is what blows a tournament frame
   limit, so p99.9 and max matter more than p50 here.
   Built and run by run-transport-bench.sh; argv[1] = competing CPU hogs. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <linux/futex.h>

static double now_ns(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1e9+t.tv_nsec; }
static int cmpd(const void*a,const void*b){ double x=*(const double*)a,y=*(const double*)b; return x<y?-1:x>y; }
static long fwait(atomic_int*a,int v){ return syscall(SYS_futex,a,FUTEX_WAIT,v,NULL,NULL,0); }
static long fwake(atomic_int*a){ return syscall(SYS_futex,a,FUTEX_WAKE,1,NULL,NULL,0); }

#define N 30000
#define WARM 2000
static double s[N];
static void report(const char*n,int c){
  qsort(s,c,sizeof(double),cmpd);
  printf("  %-26s p50 %7.2f  p99 %8.2f  p99.9 %9.2f  max %10.2f us\n",
    n, s[c/2]/1e3, s[(int)(c*0.99)]/1e3, s[(int)(c*0.999)]/1e3, s[c-1]/1e3);
}

/* Wait for *w to reach `target`. One load feeds both the comparison and the
   futex value -- loading twice races: the compare sees stale, the futex arg
   sees fresh, and the wait sleeps on a value nobody will wake again. */
static void wait_for(atomic_int*w,int target,int mode,int spin){
  int k=0;
  for(;;){
    int cur = atomic_load(w);
    if(cur>=target) return;
    if(mode==0){ __builtin_ia32_pause(); continue; }              /* pure spin */
    if(mode==1 && k++<spin){ __builtin_ia32_pause(); continue; }  /* spin, then block */
    fwait(w,cur);
  }
}

/* mode 0=spin  1=spin-then-futex  2=futex only  3=socketpair */
static void run(const char*name,int mode,int spin){
  atomic_int *m = mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);
  atomic_store(&m[0],0); atomic_store(&m[1],0);
  int sv[2]; socketpair(AF_UNIX,SOCK_STREAM,0,sv);
  pid_t p=fork();
  if(p==0){
    if(mode==3){ char c; for(int i=0;i<N;i++){ if(read(sv[1],&c,1)!=1)_exit(0); if(write(sv[1],&c,1)!=1)_exit(0);} _exit(0); }
    for(int i=1;i<=N;i++){ wait_for(&m[0],i,mode,spin); atomic_store(&m[1],i); if(mode) fwake(&m[1]); }
    _exit(0);
  }
  int c=0;
  for(int i=1;i<=N;i++){
    double t=now_ns();
    if(mode==3){ char x=1; if(write(sv[0],&x,1)!=1||read(sv[0],&x,1)!=1) break; }
    else{ atomic_store(&m[0],i); if(mode) fwake(&m[0]); wait_for(&m[1],i,mode,spin); }
    if(i>WARM) s[c++]=now_ns()-t;
  }
  report(name,c);
  waitpid(p,NULL,0); close(sv[0]); close(sv[1]); munmap((void*)m,4096);
}

int main(int argc,char**argv){
  setvbuf(stdout,NULL,_IOLBF,0);
  int load = argc>1 ? atoi(argv[1]) : 0;
  pid_t lp[64];
  if(load>64) load=64;
  for(int i=0;i<load;i++){ if((lp[i]=fork())==0){ volatile double x=0; for(;;) x+=1.0000001; } }
  printf("== frame handoff round trip  (%d competing CPU hogs) ==\n", load);
  run("shm + pure spin",        0,0);
  run("shm + spin 2000, futex", 1,2000);
  run("shm + spin 200,  futex", 1,200);
  run("shm + futex only",       2,0);
  run("unix socketpair",        3,0);
  for(int i=0;i<load;i++){ kill(lp[i],SIGKILL); waitpid(lp[i],NULL,0); }
  return 0;
}
