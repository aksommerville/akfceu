/* akfceu_file.h
 */

#ifndef AKFCEU_FILE_H
#define AKFCEU_FILE_H

int akfceu_file_read(void *dstpp,const char *path);
int akfceu_file_write(const char *path,const void *src,int srcc);

struct akfceu_line_reader {
  const char *src;
  int srcc;
  int srcp;
  const char *line;
  int linec;
  int lineno;
};

int akfceu_line_reader_next(struct akfceu_line_reader *reader);

int akfceu_int_eval(int *dst,const char *src,int srcc);

#endif
