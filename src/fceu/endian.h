#ifndef FCEU_ENDIAN_H
#define FCEU_ENDIAN_H

int write16le(uint16_t b, FILE *fp);
int write32le(uint32_t b, FILE *fp);
int read32le(uint32_t *Bufo, FILE *fp);
void FlipByteOrder(uint8_t *src, uint32_t count);

void FCEU_en32lsb(uint8_t *, uint32_t);
uint32_t FCEU_de32lsb(uint8_t *);

#endif
