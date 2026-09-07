/* R12: the packetization tax -- what a datagram protocol (QUIC) pays that a
   stream protocol (TCP) does not, for the same frame-sized payload. Send-side
   only: the receiver drains and drops, so this measures the cost of getting
   bytes out the door, which is exactly the quantity in question.
   Built and run by run-transport-bench.sh. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#ifndef UDP_SEGMENT
#define UDP_SEGMENT 103
#endif

static double now_ns(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1e9+t.tv_nsec; }
#define PAY   571200      /* 1700 units x 336 B: the worst realistic frame */
#define MTU     1200      /* QUIC's conservative datagram size */
#define NPKT  ((PAY+MTU-1)/MTU)
#define ITERS    300

int main(void){
  setvbuf(stdout,NULL,_IOLBF,0);
  char *buf = malloc(PAY); memset(buf,0xA5,PAY);
  printf("payload %d B  ->  %d datagrams at %d B (QUIC-style)\n\n", PAY, NPKT, MTU);

  { /* TCP: one write, kernel segments via TSO */
    int l=socket(AF_INET,SOCK_STREAM,0), one=1;
    setsockopt(l,SOL_SOCKET,SO_REUSEADDR,&one,4);
    struct sockaddr_in sa={.sin_family=AF_INET,.sin_addr={htonl(INADDR_LOOPBACK)}};
    bind(l,(void*)&sa,sizeof sa); listen(l,1); socklen_t sl=sizeof sa; getsockname(l,(void*)&sa,&sl);
    int c=socket(AF_INET,SOCK_STREAM,0); connect(c,(void*)&sa,sizeof sa);
    int srv=accept(l,NULL,NULL); close(l);
    setsockopt(c,IPPROTO_TCP,TCP_NODELAY,&one,4);
    pid_t p=fork();
    if(p==0){ char*r=malloc(PAY); for(;;){ size_t g=0; while(g<PAY){ ssize_t n=read(srv,r+g,PAY-g); if(n<=0)_exit(0); g+=n; } } }
    double t=now_ns();
    for(int i=0;i<ITERS;i++){ size_t w=0; while(w<PAY){ ssize_t n=write(c,buf+w,PAY-w); if(n<=0) break; w+=n; } }
    double e=(now_ns()-t)/ITERS;
    printf("  TCP   1 stream write (kernel TSO)   %8.1f us/frame   %5.2f GB/s   syscalls/frame %4d\n",
           e/1000, PAY/(e/1e9)/1e9, 1);
    close(c); kill(p,SIGKILL); waitpid(p,NULL,0); }

  { /* UDP: one sendmsg per datagram -- what an untuned QUIC does */
    int c=socket(AF_INET,SOCK_DGRAM,0), srv=socket(AF_INET,SOCK_DGRAM,0);
    struct sockaddr_in sa={.sin_family=AF_INET,.sin_addr={htonl(INADDR_LOOPBACK)}};
    bind(srv,(void*)&sa,sizeof sa); socklen_t sl=sizeof sa; getsockname(srv,(void*)&sa,&sl);
    int rb=16<<20; setsockopt(srv,SOL_SOCKET,SO_RCVBUF,&rb,4); setsockopt(c,SOL_SOCKET,SO_SNDBUF,&rb,4);
    connect(c,(void*)&sa,sizeof sa);
    pid_t p=fork();
    if(p==0){ char r[2048]; for(;;) if(recv(srv,r,sizeof r,0)<=0) _exit(0); }
    double t=now_ns();
    for(int i=0;i<ITERS;i++) for(int k=0;k<NPKT;k++){ size_t n=(k==NPKT-1)?PAY-(size_t)k*MTU:MTU; send(c,buf+(size_t)k*MTU,n,0); }
    double e=(now_ns()-t)/ITERS;
    printf("  UDP   1 send() per datagram         %8.1f us/frame   %5.2f GB/s   syscalls/frame %4d\n",
           e/1000, PAY/(e/1e9)/1e9, NPKT);
    close(c); kill(p,SIGKILL); waitpid(p,NULL,0); }

  { /* UDP + GSO: 64 segments per syscall -- what a tuned QUIC does, on Linux */
    int c=socket(AF_INET,SOCK_DGRAM,0), srv=socket(AF_INET,SOCK_DGRAM,0);
    struct sockaddr_in sa={.sin_family=AF_INET,.sin_addr={htonl(INADDR_LOOPBACK)}};
    bind(srv,(void*)&sa,sizeof sa); socklen_t sl=sizeof sa; getsockname(srv,(void*)&sa,&sl);
    int rb=16<<20; setsockopt(srv,SOL_SOCKET,SO_RCVBUF,&rb,4); setsockopt(c,SOL_SOCKET,SO_SNDBUF,&rb,4);
    connect(c,(void*)&sa,sizeof sa);
    int gso=MTU;
    if(setsockopt(c,IPPROTO_UDP,UDP_SEGMENT,&gso,sizeof gso)<0){ puts("  UDP+GSO unavailable on this kernel"); return 0; }
    pid_t p=fork();
    if(p==0){ char r[2048]; for(;;) if(recv(srv,r,sizeof r,0)<=0) _exit(0); }
    const int BATCH=64*MTU;
    double t=now_ns();
    for(int i=0;i<ITERS;i++){ size_t off=0; while(off<PAY){ size_t n=PAY-off; if(n>BATCH) n=BATCH; send(c,buf+off,n,0); off+=n; } }
    double e=(now_ns()-t)/ITERS;
    printf("  UDP   send() + GSO, 64 seg/syscall  %8.1f us/frame   %5.2f GB/s   syscalls/frame %4d\n",
           e/1000, PAY/(e/1e9)/1e9, (PAY+BATCH-1)/BATCH);
    close(c); kill(p,SIGKILL); waitpid(p,NULL,0); }
  return 0;
}
