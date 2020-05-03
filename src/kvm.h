/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2013 Mike Chambers
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

#ifndef FAKE86_KVM_H_INCLUDED
#define FAKE86_KVM_H_INCLUDED
#include "config.h"
#ifdef USE_KVM

#include <linux/kvm.h>
#include <stdint.h>

struct kvm {
        int     kvm_fd;
        int     vm_fd;
        int     vcpu_fd;
        uint8_t *mem;
	size_t	mem_size;
        struct kvm_run   *kvm_run;
        struct kvm_sregs sregs;
        struct kvm_regs  regs;
};

extern struct kvm kvm;

extern int  kvm_init   ( size_t mem_size );
extern void kvm_uninit ( void );
extern int  kvm_run    ( void );

// For now, only 16 bit registers and stuffs before the 386 era is populated for now ...

static inline uint16_t KVM_GET_CS ( void       ) { return kvm.sregs.cs.selector; }
static inline uint16_t KVM_GET_DS ( void       ) { return kvm.sregs.ds.selector; }
static inline uint16_t KVM_GET_ES ( void       ) { return kvm.sregs.es.selector; }
static inline uint16_t KVM_GET_SS ( void       ) { return kvm.sregs.ss.selector; }

static inline void     KVM_SET_CS ( uint16_t v ) { kvm.sregs.cs.selector = v; kvm.sregs.cs.base = v << 4; }
static inline void     KVM_SET_DS ( uint16_t v ) { kvm.sregs.ds.selector = v; kvm.sregs.ds.base = v << 4; }
static inline void     KVM_SET_ES ( uint16_t v ) { kvm.sregs.es.selector = v; kvm.sregs.es.base = v << 4; }
static inline void     KVM_SET_SS ( uint16_t v ) { kvm.sregs.ss.selector = v; kvm.sregs.ss.base = v << 4; }

static inline uint16_t KVM_GET_AX ( void       ) { return kvm.regs.rax; }
static inline uint16_t KVM_GET_BX ( void       ) { return kvm.regs.rbx; }
static inline uint16_t KVM_GET_CX ( void       ) { return kvm.regs.rcx; }
static inline uint16_t KVM_GET_DX ( void       ) { return kvm.regs.rdx; }
static inline uint16_t KVM_GET_SI ( void       ) { return kvm.regs.rsi; }
static inline uint16_t KVM_GET_DI ( void       ) { return kvm.regs.rdi; }
static inline uint16_t KVM_GET_BP ( void       ) { return kvm.regs.rbp; }
static inline uint16_t KVM_GET_SP ( void       ) { return kvm.regs.rsp; }
static inline uint16_t KVM_GET_IP ( void       ) { return kvm.regs.rip; }
static inline uint16_t KVM_GET_FL ( void       ) { return kvm.regs.rflags; }

static inline void     KVM_SET_AX ( uint16_t v ) { kvm.regs.rax = (kvm.regs.rax & ~0xFFFFUL) | v; }
static inline void     KVM_SET_BX ( uint16_t v ) { kvm.regs.rbx = (kvm.regs.rbx & ~0xFFFFUL) | v; }
static inline void     KVM_SET_CX ( uint16_t v ) { kvm.regs.rcx = (kvm.regs.rcx & ~0xFFFFUL) | v; }
static inline void     KVM_SET_DX ( uint16_t v ) { kvm.regs.rdx = (kvm.regs.rdx & ~0xFFFFUL) | v; }
static inline void     KVM_SET_SI ( uint16_t v ) { kvm.regs.rsi = (kvm.regs.rsi & ~0xFFFFUL) | v; }
static inline void     KVM_SET_DI ( uint16_t v ) { kvm.regs.rdi = (kvm.regs.rdi & ~0xFFFFUL) | v; }
static inline void     KVM_SET_BP ( uint16_t v ) { kvm.regs.rbp = (kvm.regs.rbp & ~0xFFFFUL) | v; }
static inline void     KVM_SET_SP ( uint16_t v ) { kvm.regs.rsp = (kvm.regs.rsp & ~0xFFFFUL) | v; }
static inline void     KVM_SET_IP ( uint16_t v ) { kvm.regs.rip = (kvm.regs.rip & ~0xFFFFUL) | v; }
static inline void     KVM_SET_FL ( uint16_t v ) { kvm.regs.rflags = (kvm.regs.rflags & ~0xFFFFUL) | v; }

#endif
#endif
