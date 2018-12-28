int FCEU_InitVirtualVideo(void);
void FCEU_KillVirtualVideo(void);
int SaveSnapshot(void);
extern uint8_t *XBuf;
void FCEU_DrawNumberRow(uint8_t *XBuf, int *nstatus, int cur);
