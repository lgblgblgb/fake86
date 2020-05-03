/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2012 Mike Chambers
            (C)2020      Gabor Lenart "LGB"

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef FAKE86_PARSECL_H_INCLUDED
#define FAKE86_PARSECL_H_INCLUDED

#include "config.h"

extern uint16_t constanth;                                                                                                                                                                                           
extern uint16_t constantw;
extern uint8_t slowsystem;
extern char *biosfile;
extern uint32_t speed;
extern uint8_t verbose;
extern uint8_t useconsole;
extern uint8_t usessource;
#ifdef USE_KVM
extern int usekvm;
#endif

extern uint8_t dohardreset;

extern void parsecl ( int argc, char *argv[] );
extern uint32_t loadrom ( uint32_t addr32, const char *filename, uint8_t failure_fatal );

#endif
