#include "ra_client.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* I/O buffer.
 */

struct ra_queue {
  char *v;
  int p,c,a;
};

static void ra_queue_cleanup(struct ra_queue *q) {
  if (q->v) free(q->v);
}

static int ra_queue_require(struct ra_queue *q,int addc) {
  if (addc<1) return 0;
  if (q->c>INT_MAX-addc) return -1;
  int na=q->c+addc;
  if (q->p<=q->a-na) return 0;
  if (q->p) {
    memmove(q->v,q->v+q->p,q->c);
    q->p=0;
    if (na<=q->a) return 0;
  }
  if (na<INT_MAX-1024) na=(na+1024)&~1023;
  void *nv=realloc(q->v,na);
  if (!nv) return -1;
  q->v=nv;
  q->a=na;
  return 0;
}

static int ra_queue_append(struct ra_queue *q,const void *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (ra_queue_require(q,srcc)<0) return -1;
  memcpy(q->v+q->p+q->c,src,srcc);
  q->c+=srcc;
  return 0;
}

static inline int ra_queue_available(const struct ra_queue *q) {
  return q->a-q->c-q->p;
}

static inline void ra_queue_consume(struct ra_queue *q,int c) {
  if (c>q->c) c=q->c;
  if (!(q->c-=c)) q->p=0;
  else q->p+=c;
}

/* Object definition.
 */

struct ra_client {
  int fd;
  struct romassist_delegate delegate;
  struct ra_queue rbuf;
  struct ra_queue wbuf;
};

/* New.
 */
 
romassist_t romassist_new(
  const void *ipv4addr,int port,
  const struct romassist_delegate *delegate
) {
  if (!ipv4addr) ipv4addr="\x7f\x00\x00\x01";
  if ((port<1)||(port>0xffff)) port=8111;
  romassist_t client=calloc(1,sizeof(struct ra_client));
  if (!client) return 0;

  if ((client->fd=socket(PF_INET,SOCK_STREAM,IPPROTO_TCP))<0) {
    romassist_del(client);
    return 0;
  }

  struct sockaddr_in saddr={
    .sin_family=AF_INET,
    .sin_port=htons(port),
  };
  memcpy(&saddr.sin_addr,ipv4addr,4);
  if (connect(client->fd,(struct sockaddr*)&saddr,sizeof(saddr))<0) {
    romassist_del(client);
    return 0;
  }

  if (delegate) {
    memcpy(&client->delegate,delegate,sizeof(struct romassist_delegate));
  }

  if (!client->delegate.client_name) client->delegate.client_name="Anonymous";
  if (!client->delegate.platforms) client->delegate.platforms="nes,snes,gameboy,genesis,atari2600,atari7800,c64";

  if (romassist_send_hello(client)<0) {
    romassist_del(client);
    return 0;
  }

  return client;
}

/* Delete client.
 */
 
void romassist_del(romassist_t client) {
  if (!client) return;
  if (client->fd>=0) close(client->fd);
  free(client);
}

/* Dispatch one incoming packet.
 */

static int romassist_dispatch_packet(romassist_t client,int packetid,const void *src,int srcc) {
  switch (packetid) {
  
    case 0x0000: { // DUMMY
        // Nothing to do with this.
      } break;
      
    case 0x0001: { // CLIENT_HELLO
        // Server should not send this.
      } break;
      
    case 0x0002: { // LAUNCHED
        // Server should not send this.
      } break;
      
    case 0x0003: { // LAUNCH
        if (client->delegate.launch_file) {
          char tmp[1024];
          if (srcc<sizeof(tmp)) {
            memcpy(tmp,src,srcc);
            tmp[srcc]=0;
            int err=client->delegate.launch_file(client,tmp,srcc);
            if (err<0) return err;
          }
        }
      } break;
      
    case 0x0004: { // GET_DETAILS
        // Server should not send this.
      } break;
      
    case 0x0005: { // DETAILS
        if (client->delegate.details) {
          int err=client->delegate.details(client,src,srcc);
          if (err<0) return err;
        }
      } break;
      
    case 0x0006: { // COMMAND
        if (client->delegate.command) {
          int err=client->delegate.command(client,src,srcc);
          if (err<0) return err;
        }
      } break;
      
    default: {
        // Mystery packet!
      } break;
  }
  return 0;
}

/* Measure a packet on the read buffer.
 * If complete, dispatch it and return its length.
 * Incomplete, return zero.
 */

static int romassist_read_packet(romassist_t client) {
  if (client->rbuf.c<6) return 0;
  int srcc=client->rbuf.c;
  unsigned char *src=(unsigned char*)(client->rbuf.v+client->rbuf.p);
  if (src[0]!=0xff) return -1; // Missing introducer byte.
  int packetid=(src[1]<<8)|src[2];
  int payloadlen=(src[3]<<16)|(src[4]<<8)|src[5];
  int srcp=6;
  if (srcp>srcc-payloadlen) return 0;
  int err=romassist_dispatch_packet(client,packetid,src+srcp,payloadlen);
  if (err<0) return err;
  return srcp+payloadlen;
}

/* Read from socket, then pop packets off the buffer.
 */

static int romassist_update_read(romassist_t client) {
  if (ra_queue_require(&client->rbuf,1)<0) return -1;
  int err=read(client->fd,client->rbuf.v+client->rbuf.p+client->rbuf.c,ra_queue_available(&client->rbuf));
  if (err<0) {
    if (errno==EINTR) return 0;
    return -1;
  }
  if (!err) return -1;
  client->rbuf.c+=err;
  while (client->rbuf.c>0) {
    if ((err=romassist_read_packet(client))<=0) return err;
    ra_queue_consume(&client->rbuf,err);
  }
  return 0;
}

/* Write to socket from buffer.
 */

static int romassist_update_write(romassist_t client) {
  int err=write(client->fd,client->wbuf.v+client->wbuf.p,client->wbuf.c);
  if (err<=0) return -1;
  ra_queue_consume(&client->wbuf,err);
  return 0;
}

/* Update.
 */
 
int romassist_update(romassist_t client) {
  if (!client) return -1;
  if (client->fd<0) return -1;

  struct pollfd pollfd={.fd=client->fd,.events=POLLHUP|POLLERR};
  if (client->wbuf.c) pollfd.events|=POLLOUT;
  else pollfd.events|=POLLIN;
  
  int err=poll(&pollfd,1,0);
  if (err<0) {
    if (errno==EINTR) return 0;
    return -1;
  }
  if (!err) return 0;

  if (pollfd.revents&POLLOUT) {
    int err=romassist_update_write(client);
    if (err<0) return err;
  } else if (pollfd.revents&(POLLIN|POLLHUP|POLLERR)) {
    if (romassist_update_read(client)<0) return -1;
  }
  
  return 1;
}

/* Queue a packet for send.
 */

static int romassist_queue_packet(romassist_t client,int packetid,const void *src,int srcc) {
  if (!client) return -1;
  if (client->fd<0) return -1;
  if ((packetid<0)||(packetid>0xffff)) return -1;
  if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if ((srcc<0)||(srcc>0xffffff)) return -1;
  if (srcc&&!src) return -1;
  unsigned char hdr[6]={
    0xff,packetid>>8,packetid,srcc>>16,srcc>>8,srcc
  };
  if (ra_queue_append(&client->wbuf,hdr,sizeof(hdr))<0) return -1;
  if (ra_queue_append(&client->wbuf,src,srcc)<0) return -1;
  return 0;
}

/* Send LAUNCHED.
 */

int romassist_launched_file(romassist_t client,const char *path) {
  if (!client||!path) return -1;
  int pathc=0;
  while (path[pathc]) pathc++;
  if (pathc<1) return -1;
  return romassist_queue_packet(client,0x0002,path,pathc);
}

/* Send GET_DETAILS.
 */
 
int romassist_request_details(romassist_t client,const char *path) {
  if (!client||!path) return -1;
  int pathc=0;
  while (path[pathc]) pathc++;
  if (pathc<1) return -1;
  return romassist_queue_packet(client,0x0004,path,pathc);
}

/* Send CLIENT_HELLO.
 */
 
int romassist_send_hello(romassist_t client) {
  if (!client) return -1;
  
  if (ra_queue_append(&client->wbuf,"\xff\x00\x01",3)<0) return -1;
  int lenp=client->wbuf.c;
  if (ra_queue_append(&client->wbuf,"\0\0\0",3)<0) return -1;
  int bodyp=client->wbuf.c;

  if (client->delegate.client_name) {
    int c=0;
    while (client->delegate.client_name[c]) c++;
    if (c>255) c=255;
    char tmp=c;
    if (ra_queue_append(&client->wbuf,&tmp,1)<0) return -1;
    if (ra_queue_append(&client->wbuf,client->delegate.client_name,c)<0) return -1;
  } else {
    if (ra_queue_append(&client->wbuf,"\0",1)<0) return -1;
  }

  if (client->delegate.platforms) {
    const char *src=client->delegate.platforms;
    int srcp=0;
    while (src[srcp]) {
      if (src[srcp]==',') { srcp++; continue; }
      const char *token=src+srcp;
      int tokenc=0;
      while (src[srcp]&&(src[srcp]!=',')) { srcp++; tokenc++; }
      while (tokenc&&((unsigned char)token[tokenc-1]<=0x20)) tokenc--;
      while (tokenc&&((unsigned char)token[0]<=0x20)) { tokenc--; token++; }
      if (tokenc>255) tokenc=255;
      if (!tokenc) continue;
      char tmp=tokenc;
      if (ra_queue_append(&client->wbuf,&tmp,1)<0) return -1;
      if (ra_queue_append(&client->wbuf,token,tokenc)<0) return -1;
    }
  }
  if (ra_queue_append(&client->wbuf,"\0",1)<0) return -1;

  int bodyc=client->wbuf.c-bodyp;
  unsigned char *lendst=((unsigned char*)client->wbuf.v)+client->wbuf.p+lenp;
  lendst[0]=bodyc>>16;
  lendst[1]=bodyc>>8;
  lendst[2]=bodyc;

  return 0;
}
