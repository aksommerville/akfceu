#include "akfceu_file.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

/* Read entire file.
 */
 
int akfceu_file_read(void *dstpp,const char *path) {
  if (!dstpp||!path||!path[0]) return -1;
  int fd=open(path,O_RDONLY);
  if (fd<0) return -1;
  off_t flen=lseek(fd,0,SEEK_END);
  if ((flen<0)||(flen>INT_MAX)||lseek(fd,0,SEEK_SET)) {
    close(fd);
    return -1;
  }
  char *dst=malloc(flen);
  if (!dst) {
    close(fd);
    return -1;
  }
  int dstc=0;
  while (dstc<flen) {
    int err=read(fd,dst+dstc,flen-dstc);
    if (err<=0) {
      close(fd);
      free(dst);
      return -1;
    }
    dstc+=err;
  }
  close(fd);
  *(void**)dstpp=dst;
  return dstc;
}

/* Advance line reader.
 */

int akfceu_line_reader_next(struct akfceu_line_reader *reader) {
  if (!reader) return 0;
  while (reader->srcp<reader->srcc) {
    reader->line=reader->src+reader->srcp;
    reader->linec=0;
    reader->lineno++;
    int comment=0;
    while (reader->srcp<reader->srcc) {
      if (reader->src[reader->srcp]==0x0a) {
        reader->srcp++;
        break;
      }
      if (comment) {
      } else if (reader->src[reader->srcp]=='#') {
        comment=1;
      } else {
        reader->linec++;
      }
      reader->srcp++;
    }
    while (reader->linec&&((unsigned char)reader->line[0]<=0x20)) { reader->line++; reader->linec--; }
    while (reader->linec&&((unsigned char)reader->line[reader->linec-1]<=0x20)) reader->linec--;
    if (reader->linec) return 1;
  }
  return 0;
}

/* Evaluate integer.
 */

int akfceu_int_eval(int *dst,const char *src,int srcc) {
  if (!dst||!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0,positive=1,base=10;

  if (srcp>=srcc) return -1;
  if (src[srcp]=='-') {
    positive=0;
    if (++srcp>=srcc) return -1;
  } else if (src[srcp]=='+') {
    if (++srcp>=srcc) return -1;
  }

  if ((srcp<srcc-2)&&(src[srcp]=='0')) switch (src[srcp+1]) {
    case 'b': case 'B': base=2; srcp+=2; break;
    case 'o': case 'O': base=8; srcp+=2; break;
    case 'd': case 'D': base=10; srcp+=2; break;
    case 'x': case 'X': base=16; srcp+=2; break;
  }

  if (srcp>=srcc) return -1;
  int limit=(positive?(INT_MAX/base):(INT_MIN/base));
  *dst=0;
  
  while (srcp<srcc) {
    int digit=src[srcp++];
         if ((digit>='0')&&(digit<='9')) digit=digit-'0';
    else if ((digit>='a')&&(digit<='z')) digit=digit-'a'+10;
    else if ((digit>='A')&&(digit<='Z')) digit=digit-'A'+10;
    else return -1;
    if (digit>=base) return -1;
    if (positive) {
      if (*dst>limit) return -1;
      (*dst)*=base;
      if (*dst>INT_MAX-digit) return -1;
      (*dst)+=digit;
    } else {
      if (*dst<limit) return -1;
      (*dst)*=base;
      if (*dst<INT_MIN+digit) return -1;
      (*dst)-=digit;
    }
  }

  return 0;
}
