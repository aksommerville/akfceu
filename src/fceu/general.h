#ifndef FCEU_GENERAL_H
#define FCEU_GENERAL_H

void GetFileBase(const char *f);
extern uint32_t uppow2(uint32_t n);

int FCEU_MakePath(char *dst,int dsta,int type,int id1,const char *cd1);

#define FCEUMKF_STATE    1
#define FCEUMKF_SAV      3
#define FCEUMKF_FDSROM   5
#define FCEUMKF_PALETTE  6
#define FCEUMKF_IPS      8
#define FCEUMKF_FDS      9
#define FCEUMKF_CONFIG  12

#endif
