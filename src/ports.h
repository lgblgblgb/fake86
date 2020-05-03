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

#ifndef FAKE86_PORTS_H_INCLUDED
#define FAKE86_PORTS_H_INCLUDED

#include <stdint.h>

typedef void     (*io_write8_cb_t)  (uint16_t portnum, uint8_t value);
typedef uint8_t  (*io_read8_cb_t)   (uint16_t portnum);
typedef void     (*io_write16_cb_t) (uint16_t portnum, uint16_t value);
typedef uint16_t (*io_read16_cb_t)  (uint16_t portnum);

extern uint8_t portram[0x10000];

extern void set_port_write_redirector (uint16_t startport, uint16_t endport, io_write8_cb_t callback);
extern void set_port_read_redirector (uint16_t startport, uint16_t endport, io_read8_cb_t callback);
// Seems these are not used??
//extern void set_port_write_redirector_16 (uint16_t startport, uint16_t endport, io_write16_cb_t callback);
//extern void set_port_read_redirector_16 (uint16_t startport, uint16_t endport, io_read16_cb_t callback);

extern uint16_t portin16(uint16_t portnum);
extern uint8_t portin(uint16_t portnum);
extern void portout16(uint16_t portnum, uint16_t value);
extern void portout(uint16_t portnum, uint8_t value);
extern void ports_init ( void );

#endif
