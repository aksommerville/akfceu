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

typedef struct {
     void (*Fill)(int Count);  /* Low quality ext sound. */

     /* NeoFill is for sound devices that are emulated in a more
        high-level manner(VRC7) in HQ mode.  Interestingly,
        this device has slightly better sound quality(updated more
        often) in lq mode than in high-quality mode.  Maybe that
        should be fixed. :)
     */
     void (*NeoFill)(int32_t *Wave, int Count);
     void (*HiFill)(void);
     void (*HiSync)(int32_t ts);

     void (*RChange)(void);
     void (*Kill)(void);
} EXPSOUND;

extern EXPSOUND GameExpSound;

extern int32_t nesincsize;

void SetSoundVariables(void);

int GetSoundBuffer(int32_t **W);
int FlushEmulateSound(void);
extern int32_t Wave[2048+512];
extern int32_t WaveFinal[2048+512];
extern int32_t WaveHi[];
extern uint32_t soundtsinc;

extern uint32_t soundtsoffs;
#define SOUNDTS (timestamp + soundtsoffs)

void SetNESSoundMap(void);
void FrameSoundUpdate(void);

void FCEUSND_Power(void);
void FCEUSND_Reset(void);
void FCEUSND_SaveState(void);
void FCEUSND_LoadState(int version);

void FASTAPASS(1) FCEU_SoundCPUHook(int);
