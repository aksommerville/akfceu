typedef struct {
  void *fp;      // FILE* or ptr to ZIPWRAP
  uint32_t type; // 0=normal file, 1=gzip, 2=zip
} FCEUFILE;

FCEUFILE *FCEU_fopen(const char *path, const char *ipsfn, char *mode, char *ext);
int FCEU_fclose(FCEUFILE*);
uint64_t FCEU_fread(void *ptr, size_t size, size_t nmemb, FCEUFILE*);
uint64_t FCEU_fwrite(void *ptr, size_t size, size_t nmemb, FCEUFILE*);
int FCEU_fseek(FCEUFILE*, long offset, int whence);
uint64_t FCEU_ftell(FCEUFILE*);
void FCEU_rewind(FCEUFILE*);
int FCEU_read32le(uint32_t *Bufo, FCEUFILE*);
int FCEU_read16le(uint16_t *Bufo, FCEUFILE*);
int FCEU_fgetc(FCEUFILE*);
uint64_t FCEU_fgetsize(FCEUFILE*);
int FCEU_fisarchive(FCEUFILE*);

