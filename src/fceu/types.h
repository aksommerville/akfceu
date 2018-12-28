/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2001 Aaron Oneal
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

#ifndef __FCEU_TYPES
#define __FCEU_TYPES

#include <stdint.h>

#ifdef __GNUC__
  #define INLINE inline
  #define GINLINE inline
#elif MSVC
  #define INLINE __inline
  #define GINLINE /* Can't declare a function INLINE and global in MSVC.  Bummer. */
  #define PSS_STYLE 2 /* Does MSVC compile for anything other than Windows/DOS targets? */
#endif

#if PSS_STYLE==2
  #define PSS "\\"
  #define PS '\\'
#elif PSS_STYLE==1
  #define PSS "/"
  #define PS '/'
#elif PSS_STYLE==3
  #define PSS "\\"
  #define PS '\\'
#elif PSS_STYLE==4
  #define PSS ":"
  #define PS ':'
#endif

#ifdef __GNUC__
 #ifdef C80x86
  #define FASTAPASS(x) __attribute__((regparm(x)))
  #define FP_FASTAPASS FASTAPASS
 #else
  #define FASTAPASS(x) 
  #define FP_FASTAPASS(x) 
 #endif
#elif MSVC
 #define FP_FASTAPASS(x)
 #define FASTAPASS(x) __fastcall
#else
 #define FP_FASTAPASS(x)
 #define FASTAPASS(x)
#endif

typedef void (FP_FASTAPASS(2) *writefunc)(uint32_t A, uint8_t V);
typedef uint8_t (FP_FASTAPASS(1) *readfunc)(uint32_t A);

#endif
