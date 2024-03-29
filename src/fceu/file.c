/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2002 Xodnizel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <zlib.h>
#include "unzip.h"

#include "types.h"
#include "file.h"
#include "endian.h"
#include "memory.h"
#include "driver.h"
#include "general.h"

typedef struct {
           uint8_t *data;
           uint32_t size;
           uint32_t location;
} MEMWRAP;

void ApplyIPS(FILE *ips, MEMWRAP *dest)
{
 uint8_t header[5];
 uint32_t count=0;

 FCEU_printf(" Applying IPS...\n");
 if (fread(header,1,5,ips)!=5)
 {
  fclose(ips);
  return;
 }
 if (memcmp(header,"PATCH",5))
 {
  fclose(ips);
  return;
 }

 while (fread(header,1,3,ips)==3)
 {
  uint32_t offset=(header[0]<<16)|(header[1]<<8)|header[2];
  uint16_t size;

  if (!memcmp(header,"EOF",3))
  {
   FCEU_printf(" IPS EOF:  Did %d patches\n\n",count);
   fclose(ips);
   return;
  }

  size=fgetc(ips)<<8;
  size|=fgetc(ips);
  if (!size)  /* RLE */
  {
   uint8_t *start;
   uint8_t b;
   size=fgetc(ips)<<8;
   size|=fgetc(ips);

   //FCEU_printf("  Offset: %8d  Size: %5d RLE\n",offset,size);

   if ((offset+size)>dest->size)
   {
    uint8_t *tmp;

    // Probably a little slow.
    tmp=(uint8_t *)realloc(dest->data,offset+size);
    if (!tmp)
    {
     FCEU_printf("  Oops.  IPS patch %d(type RLE) goes beyond end of file.  Could not allocate memory.\n",count);
     fclose(ips);
     return;
    }
    dest->size=offset+size;
    dest->data=tmp;
    memset(dest->data+dest->size,0,offset+size-dest->size);
   }
   b=fgetc(ips);
   start=dest->data+offset;
   do
   {
    *start=b;
    start++;
   } while (--size);
  }
  else    /* Normal patch */
  {
   //FCEU_printf("  Offset: %8d  Size: %5d\n",offset,size);
   if ((offset+size)>dest->size)
   {
    uint8_t *tmp;
   
    // Probably a little slow.
    tmp=(uint8_t *)realloc(dest->data,offset+size);
    if (!tmp)
    {
     FCEU_printf("  Oops.  IPS patch %d(type normal) goes beyond end of file.  Could not allocate memory.\n",count);
     fclose(ips);
     return;
    }
    dest->data=tmp;
    memset(dest->data+dest->size,0,offset+size-dest->size);
   }
   fread(dest->data+offset,1,size,ips);
  }
  count++;
 }
 fclose(ips);
 FCEU_printf(" Hard IPS end!\n");
}

static MEMWRAP *MakeMemWrap(void *tz, int type)
{
 MEMWRAP *tmp;

 if (!(tmp=(MEMWRAP *)FCEU_malloc(sizeof(MEMWRAP))))
  goto doret;
 tmp->location=0;

 if (type==0)
 {
  fseek((FILE *)tz,0,SEEK_END);
  tmp->size=ftell((FILE *)tz);
  fseek((FILE *)tz,0,SEEK_SET);
  if (!(tmp->data=(uint8_t*)FCEU_malloc(tmp->size)))
  {
   free(tmp);
   tmp=0;
   goto doret;
  }
  fread(tmp->data,1,tmp->size,(FILE *)tz);
 }
 else if (type==1)
 {
  /* Bleck.  The gzip file format has the size of the uncompressed data,
     but I can't get to the info with the zlib interface(?). */
  gzFile gztz=tz;
  #define tz gztz
  for (tmp->size=0; gzgetc(tz) != EOF; tmp->size++);
  gzseek(tz,0,SEEK_SET);
  if (!(tmp->data=(uint8_t *)FCEU_malloc(tmp->size)))
  {
   free(tmp);
   tmp=0;
   goto doret;
  }
  gzread(tz,tmp->data,tmp->size);
  #undef tz
 }
 else if (type==2)
 {
  unz_file_info ufo;
  unzGetCurrentFileInfo(tz,&ufo,0,0,0,0,0,0); 

  tmp->size=ufo.uncompressed_size;
  if (!(tmp->data=(uint8_t *)FCEU_malloc(ufo.uncompressed_size)))
  {
   free(tmp);
   tmp=0;
   goto doret;
  }
  unzReadCurrentFile(tz,tmp->data,ufo.uncompressed_size);
 }

 doret:
 if (type==0)
 {
  fclose((FILE *)tz);
 }
 else if (type==1)
 {
  gzclose(tz);
 }
 else if (type==2)
 {
  unzCloseCurrentFile(tz);
  unzClose(tz);
 }
 return tmp;
}

#ifndef __GNUC__
 #define strcasecmp strcmp
#endif


FCEUFILE * FCEU_fopen(const char *path, const char *ipsfn, char *mode, char *ext)
{
 FILE *ipsfile=0;
 FCEUFILE *fceufp;
 void *t;

 if (strchr(mode,'r'))
  ipsfile=FCEUD_UTF8fopen(ipsfn,"rb");

 fceufp=(FCEUFILE *)malloc(sizeof(FCEUFILE));

 {
  unzFile tz;
  if ((tz=unzOpen(path)))  // If it's not a zip file, use regular file handlers.
        // Assuming file type by extension usually works,
        // but I don't like it. :)
  {
   if (unzGoToFirstFile(tz)==UNZ_OK)
   {
    for (;;)
    {
     char tempu[512];  // Longer filenames might be possible, but I don't
       // think people would name files that long in zip files...
     unzGetCurrentFileInfo(tz,0,tempu,512,0,0,0,0);
     tempu[511]=0;
     if (strlen(tempu)>=4)
     {
      char *za=tempu+strlen(tempu)-4;

      if (!ext)
      {
       if (!strcasecmp(za,".nes") || !strcasecmp(za,".fds") ||
          !strcasecmp(za,".nsf") || !strcasecmp(za,".unf") ||
          !strcasecmp(za,".nez"))
        break;
      }
      else if (!strcasecmp(za,ext))
       break;
     }
     if (strlen(tempu)>=5)
     {
      if (!strcasecmp(tempu+strlen(tempu)-5,".unif"))
       break;
     }
     if (unzGoToNextFile(tz)!=UNZ_OK)
     {
      if (unzGoToFirstFile(tz)!=UNZ_OK) goto zpfail;
      break;    
     }
    }
    if (unzOpenCurrentFile(tz)!=UNZ_OK)
     goto zpfail;      
   }
   else
   {
    zpfail:
    free(fceufp);
    unzClose(tz);
    return 0;
   }
   if (!(fceufp->fp=MakeMemWrap(tz,2)))
   {
    free(fceufp);
    return(0);
   }
   fceufp->type=2;
   if (ipsfile)
    ApplyIPS(ipsfile,(MEMWRAP *)fceufp->fp);
   return(fceufp);
  }
 }

 if ((t=FCEUD_UTF8fopen(path,"rb")))
 {
  uint32_t magic;

  magic=fgetc((FILE *)t);
  magic|=fgetc((FILE *)t)<<8;
  magic|=fgetc((FILE *)t)<<16;

  if (magic!=0x088b1f)   /* Not gzip... */
   fclose((FILE *)t);
  else                  /* Probably gzip */
  {
   int fd;

   fd = dup(fileno( (FILE *)t));

   fclose(t);

   lseek(fd, 0, SEEK_SET);

   if ((t=gzdopen(fd,mode)))
   {
    fceufp->type=1;
    fceufp->fp=t;
    if (ipsfile)
    {
     fceufp->fp=MakeMemWrap(t,1);
     gzclose(t);

     if (fceufp->fp)
     {
      free(fceufp);
      return(0);
     }

     fceufp->type=3;
     ApplyIPS(ipsfile,(MEMWRAP *)fceufp->fp);
    }
    return(fceufp);
   }
   close(fd);
  }

 }
 
  if ((t=FCEUD_UTF8fopen(path,mode)))
  {
   fseek((FILE *)t,0,SEEK_SET);
   fceufp->type=0;
   fceufp->fp=t;
   if (ipsfile)
   {
    if (!(fceufp->fp=MakeMemWrap(t,0)))
    {
     free(fceufp);
     return(0);
    }
    fceufp->type=3;
    ApplyIPS(ipsfile,(MEMWRAP *)fceufp->fp);
   }
   return(fceufp);
  }

 free(fceufp);
 return 0;
}

int FCEU_fclose(FCEUFILE *fp)
{
 if (fp->type==1)
 {
  gzclose(fp->fp);
 }
 else if (fp->type>=2)
 {
  free(((MEMWRAP*)(fp->fp))->data);
  free(fp->fp);
 }
 else
 {
  fclose((FILE *)fp->fp);
 }
 free(fp);
 return 1;
}

uint64_t FCEU_fread(void *ptr, size_t size, size_t nmemb, FCEUFILE *fp)
{
 if (fp->type==1)
 {
  return gzread(fp->fp,ptr,size*nmemb);
 }
 else if (fp->type>=2)
 {
  MEMWRAP *wz;
  uint32_t total=size*nmemb;

  wz=(MEMWRAP*)fp->fp;
  if (wz->location>=wz->size) return 0;

  if ((wz->location+total)>wz->size)
  {
   int ak=wz->size-wz->location;
   memcpy((uint8_t*)ptr,wz->data+wz->location,ak);
   wz->location=wz->size;
   return(ak/size);
  }
  else
  {
   memcpy((uint8_t*)ptr,wz->data+wz->location,total);
   wz->location+=total;
   return nmemb;
  }
 }
 else
 {
 return fread(ptr,size,nmemb,(FILE *)fp->fp);
 }
}

uint64_t FCEU_fwrite(void *ptr, size_t size, size_t nmemb, FCEUFILE *fp)
{
 if (fp->type==1)
 {
  return gzwrite(fp->fp,ptr,size*nmemb);
 }
 else if (fp->type>=2)
 {
  return 0;
 }
 else
  return fwrite(ptr,size,nmemb,(FILE *)fp->fp);
}

int FCEU_fseek(FCEUFILE *fp, long offset, int whence)
{
 if (fp->type==1)
 {
  return( (gzseek(fp->fp,offset,whence)>0)?0:-1);
 }
 else if (fp->type>=2)
 {
  MEMWRAP *wz;
  wz=(MEMWRAP*)fp->fp;

  switch (whence)
  {
   case SEEK_SET:if(offset>=wz->size)
                  return(-1);
                 wz->location=offset;break;
   case SEEK_CUR:if(offset+wz->location>wz->size)
                  return (-1);
                 wz->location+=offset;
                 break;
  }   
  return 0;
 }
 else
  return fseek((FILE *)fp->fp,offset,whence);
}

uint64_t FCEU_ftell(FCEUFILE *fp)
{
 if (fp->type==1)
 {
  return gztell(fp->fp);
 }
 else if (fp->type>=2)
 {
  return (((MEMWRAP *)(fp->fp))->location); 
 }
 else
  return ftell((FILE *)fp->fp);
}

void FCEU_rewind(FCEUFILE *fp)
{
 if (fp->type==1)
 {
  gzrewind(fp->fp);
 }
 else if (fp->type>=2)
 {
  ((MEMWRAP *)(fp->fp))->location=0;
 }
 else
  /* Rewind */
  fseek(fp->fp,0,SEEK_SET);
}

int FCEU_read16le(uint16_t *val, FCEUFILE *fp)
{
 uint8_t t[2];

 if (fp->type>=1)
 {
  if (fp->type>=2)
  {
   MEMWRAP *wz;
   wz=(MEMWRAP *)fp->fp;
   if (wz->location+2>wz->size)
    {return 0;}
   *(uint32_t *)t=*(uint32_t *)(wz->data+wz->location);
   wz->location+=2;
  }
  else if (fp->type==1)
   if (gzread(fp->fp,&t,2)!=2) return(0);
  return(1);
 }
 else
 {
  if (fread(t,1,2,(FILE *)fp->fp)!=2) return(0);
 }
 *val=t[0]|(t[1]<<8);
 return(1);
}

int FCEU_read32le(uint32_t *Bufo, FCEUFILE *fp)
{
 if (fp->type>=1)
 {
  uint8_t t[4];
  #ifndef LSB_FIRST
  uint8_t x[4];
  #endif
  if (fp->type>=2)
  {
   MEMWRAP *wz;
   wz=(MEMWRAP *)fp->fp;
   if (wz->location+4>wz->size)
    {return 0;}
   *(uint32_t *)t=*(uint32_t *)(wz->data+wz->location);
   wz->location+=4;
  }
  else if (fp->type==1)
   gzread(fp->fp,&t,4);
  #ifndef LSB_FIRST
  x[0]=t[3];
  x[1]=t[2];
  x[2]=t[1];
  x[3]=t[0];
  *(uint32_t*)Bufo=*(uint32_t*)x;
  #else
  *(uint32_t*)Bufo=*(uint32_t*)t;
  #endif
  return 1;
 }
 else
 {
  return read32le(Bufo,(FILE *)fp->fp);
 }
}

int FCEU_fgetc(FCEUFILE *fp)
{
 if (fp->type==1)
  return gzgetc((gzFile)(fp->fp));
 else if (fp->type>=2)
 {
  MEMWRAP *wz;
  wz=(MEMWRAP *)fp->fp;
  if (wz->location<wz->size)
   return wz->data[wz->location++];
  return EOF;
 }
 else
  return fgetc((FILE *)fp->fp);
}

uint64_t FCEU_fgetsize(FCEUFILE *fp)
{
 if (fp->type==1)
 {
  gzFile gzf=fp->fp;
  int x,t;
  t=gztell(gzf);
  gzrewind(gzf);
  for (x=0; gzgetc(gzf) != EOF; x++);
  gzseek(gzf,t,SEEK_SET);
  return(x);
 }
 else if (fp->type>=2)
  return ((MEMWRAP*)(fp->fp))->size;
 else
 {
  long t,r;
  t=ftell((FILE *)fp->fp);
  fseek((FILE *)fp->fp,0,SEEK_END);
  r=ftell((FILE *)fp->fp);
  fseek((FILE *)fp->fp,t,SEEK_SET);
  return r;
 }
}

int FCEU_fisarchive(FCEUFILE *fp)
{
 if (fp->type==2)
  return 1;
 return 0;
}
