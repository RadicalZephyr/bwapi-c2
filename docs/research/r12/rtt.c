/* R12: transport floor for a same-host BWAPI proxy.
   Part 1: per-frame handoff round trip (1 byte each way) across every IPC
   primitive a proxy could use, against the shared-memory no-syscall floor.
   Part 2: bulk payload + ack, sized to the live fraction of GameData.
   Built and run by run-transport-bench.sh. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

static double now_ns(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1e9+t.tv_nsec; }
static int cmpd(const void*a,const void*b){ double x=*(const double*)a,y=*(const double*)b; return x<y?-1:x>y; }

#define N 20000
#define WARM 200
static double s[N];
static void report(const char*name,int n){
  qsort(s,n,sizeof(double),cmpd);
  printf("  %-34s  p50 %8.2f us   p99 %8.2f us\n", name, s[n/2]/1000.0, s[(int)(n*0.99)]/1000.0);
}

/* ping-pong over any bidirectional fd pair */
static void pingpong(const char*name,int a,int b){
  pid_t p=fork();
  if(p==0){ char c; for(int i=0;i<N;i++){ if(read(b,&c,1)!=1) _exit(1); if(write(b,&c,1)!=1) _exit(1);} _exit(0); }
  char c=1;
  for(int i=0;i<WARM;i++){ if(write(a,&c,1)!=1||read(a,&c,1)!=1) break; }
  for(int i=0;i<N-WARM;i++){ double t=now_ns(); if(write(a,&c,1)!=1||read(a,&c,1)!=1) break; s[i]=now_ns()-t; }
  report(name,N-WARM);
  waitpid(p,NULL,0);
}

/* eventfd pair: the closest analogue to BWAPI's 4-byte pipe token */
static void efd_pingpong(void){
  int e1=eventfd(0,0), e2=eventfd(0,0);
  pid_t p=fork();
  if(p==0){ uint64_t v; for(int i=0;i<N;i++){ if(read(e1,&v,8)!=8) _exit(1); v=1; if(write(e2,&v,8)!=8) _exit(1);} _exit(0); }
  uint64_t v=1;
  for(int i=0;i<WARM;i++){ v=1; if(write(e1,&v,8)!=8||read(e2,&v,8)!=8) break; }
  for(int i=0;i<N-WARM;i++){ double t=now_ns(); v=1; if(write(e1,&v,8)!=8||read(e2,&v,8)!=8) break; s[i]=now_ns()-t; }
  report("eventfd pair (blocking)",N-WARM);
  waitpid(p,NULL,0);
  close(e1); close(e2);
}

/* shared memory + spin: the no-syscall floor, and what module mode approximates */
static void shm_spin(void){
  atomic_int *m = mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);
  atomic_store(&m[0],0); atomic_store(&m[1],0);
  pid_t p=fork();
  if(p==0){ for(int i=1;i<=N;i++){ while(atomic_load(&m[0])<i) __builtin_ia32_pause(); atomic_store(&m[1],i);} _exit(0); }
  for(int i=1;i<=WARM;i++){ atomic_store(&m[0],i); while(atomic_load(&m[1])<i) __builtin_ia32_pause(); }
  int k=0;
  for(int i=WARM+1;i<=N;i++){ double t=now_ns(); atomic_store(&m[0],i); while(atomic_load(&m[1])<i) __builtin_ia32_pause(); s[k++]=now_ns()-t; }
  report("shared memory + spin (no syscall)",k);
  waitpid(p,NULL,0); munmap((void*)m,4096);
}

/* bulk: push a frame-sized payload, wait for a 1-byte ack */
static void bulk(const char*name,int a,int b,size_t sz){
  char *buf=malloc(sz); memset(buf,0xA5,sz);
  pid_t p=fork();
  if(p==0){ char*r=malloc(sz); for(int i=0;i<600;i++){ size_t g=0; while(g<sz){ ssize_t n=read(b,r+g,sz-g); if(n<=0)_exit(1); g+=n;} char c=1; if(write(b,&c,1)!=1)_exit(1);} _exit(0); }
  char c;
  for(int i=0;i<100;i++){ size_t w=0; while(w<sz){ ssize_t n=write(a,buf+w,sz-w); if(n<=0) break; w+=n;} if(read(a,&c,1)!=1) break; }
  for(int i=0;i<500;i++){ double t=now_ns(); size_t w=0; while(w<sz){ ssize_t n=write(a,buf+w,sz-w); if(n<=0) break; w+=n;} if(read(a,&c,1)!=1) break; s[i]=now_ns()-t; }
  qsort(s,500,sizeof(double),cmpd);
  printf("  %-34s  p50 %8.2f us  (%6.2f MB at %6.2f GB/s)\n", name, s[250]/1000.0, sz/1e6, sz/(s[250]/1e9)/1e9);
  waitpid(p,NULL,0); free(buf);
}

static void mk_tcp(int*a,int*b,int nodelay){
  int l=socket(AF_INET,SOCK_STREAM,0), one=1;
  setsockopt(l,SOL_SOCKET,SO_REUSEADDR,&one,4);
  struct sockaddr_in sa={.sin_family=AF_INET,.sin_port=0,.sin_addr={htonl(INADDR_LOOPBACK)}};
  bind(l,(void*)&sa,sizeof sa); listen(l,1);
  socklen_t sl=sizeof sa; getsockname(l,(void*)&sa,&sl);
  int c=socket(AF_INET,SOCK_STREAM,0); connect(c,(void*)&sa,sizeof sa);
  int srv=accept(l,NULL,NULL); close(l);
  if(nodelay){ setsockopt(c,IPPROTO_TCP,TCP_NODELAY,&one,4); setsockopt(srv,IPPROTO_TCP,TCP_NODELAY,&one,4); }
  *a=c; *b=srv;
}

int main(void){
  setvbuf(stdout,NULL,_IOLBF,0);
  int a,b,f[2],g[2];
  puts("== per-frame round trip (1 byte each way) ==");
  shm_spin();
  socketpair(AF_UNIX,SOCK_STREAM,0,f); pingpong("unix socketpair",f[0],f[1]); close(f[0]); close(f[1]);
  efd_pingpong();
  if(pipe(f)==0 && pipe(g)==0){
    pid_t p=fork();
    if(p==0){ char c; for(int i=0;i<N;i++){ if(read(f[0],&c,1)!=1)_exit(1); if(write(g[1],&c,1)!=1)_exit(1);} _exit(0); }
    char c=1;
    for(int i=0;i<WARM;i++){ if(write(f[1],&c,1)!=1||read(g[0],&c,1)!=1) break; }
    for(int i=0;i<N-WARM;i++){ double t=now_ns(); if(write(f[1],&c,1)!=1||read(g[0],&c,1)!=1) break; s[i]=now_ns()-t; }
    report("anonymous pipe pair (BWAPI today)",N-WARM);
    waitpid(p,NULL,0);
  }
  mk_tcp(&a,&b,1); pingpong("TCP loopback, TCP_NODELAY",a,b); close(a); close(b);
  mk_tcp(&a,&b,0); pingpong("TCP loopback, Nagle on",a,b); close(a); close(b);

  puts("\n== bulk payload + ack (loopback: the best case a wire could ever be) ==");
  mk_tcp(&a,&b,1); bulk("134 KB  (400 live units)",a,b,134400); close(a); close(b);
  mk_tcp(&a,&b,1); bulk("571 KB  (1700 units, unitArray cap)",a,b,571200); close(a); close(b);
  mk_tcp(&a,&b,1); bulk("4.1 MB  (all per-frame slots)",a,b,4138831); close(a); close(b);
  mk_tcp(&a,&b,1); bulk("33 MB   (whole GameData)",a,b,33017032); close(a); close(b);
  return 0;
}
