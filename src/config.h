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

#ifndef FAKE86_CONFIG_H_INCLUDED
#define FAKE86_CONFIG_H_INCLUDED

// Filenames starting with # -> trying to locate file in various "well-known" directories (see hostfs.c)
// Filenames starting with @ -> locate file in the preferences directory
// Filenames otherwise       -> take filename AS-IS!!
#define DEFAULT_BIOS_FILE 	"#pcxtbios.bin"
//#define DEFAULT_FONT_FILE 	"#asciivga.dat"
#define DEFAULT_FONT_FILE	""
#define DEFAULT_ROMBASIC_FILE	"#rombasic.bin"
#define DEFAULT_VIDEOROM_FILE	"#videorom.bin"
#define DEFAULT_IDEROM_FILE	"#ide_xt.bin"

// Try to enable using KVM (only compiles support. you still need -kvm switch to use that then!).
// Note, that this will be undefined at the end of this file, if Fake86 is not built for linux
// Highly experimental, and not work at all!!!!
#define USE_KVM

#define USE_OSD

// Protect video emulation thread with a mutex when accessing screen.
// Currently it's not needed, as other parts would effect the screen from the main thread in the old Fake86 is not used anymore.
//#define USE_SCREEN_MUTEX

// On UNIX (X11) platform, XInitThreads may be needed to use multi-threaded program.
// Note: if you define this, it won't be used on Windows and OSX platforms, no need to worry.
//#define USE_XINITTHREADS

//#define DO_NOT_FORCE_INLINE
//#define DO_NOT_FORCE_UNREACHABLE

//#define DEBUG_BIOS_DATA_AREA_CPU_ACCESS


//be sure to only define ONE of the CPU_* options at any given time, or
//you will likely get some unexpected/bad results!

//#define CPU_8086
//#define CPU_186
#define CPU_V20
//#define CPU_286

#if defined(CPU_8086)
	#define CPU_CLEAR_ZF_ON_MUL
	#define CPU_ALLOW_POP_CS
#else
	#define CPU_ALLOW_ILLEGAL_OP_EXCEPTION
	#define CPU_LIMIT_SHIFT_COUNT
#endif

#if defined(CPU_V20)
	#define CPU_NO_SALC
#endif

#if defined(CPU_286) || defined(CPU_386)
	#define CPU_286_STYLE_PUSH_SP
#else
	#define CPU_SET_HIGH_FLAGS
#endif

#define TIMING_INTERVAL 15

//when USE_PREFETCH_QUEUE is defined, Fake86's CPU emulator uses a 6-byte
//read-ahead cache for opcode fetches just as a real 8086/8088 does.
//by default, i just leave this disabled because it wastes a very very
//small amount of CPU power. however, for the sake of more accurate
//emulation, it can be enabled by uncommenting the line below and recompiling.
//#define USE_PREFETCH_QUEUE

//#define CPU_ADDR_MODE_CACHE

//when compiled with network support, fake86 needs libpcap/winpcap.
//if it is disabled, the ethernet card is still emulated, but no actual
//communication is possible -- as if the ethernet cable was unplugged.
#define NETWORKING_OLDCARD //planning to support an NE2000 in the future

//when DISK_CONTROLLER_ATA is defined, fake86 will emulate a true IDE/ATA1 controller
//card. if it is disabled, emulated disk access is handled by directly intercepting
//calls to interrupt 13h.
//*WARNING* - the ATA controller is not currently complete. do not use!
//#define DISK_CONTROLLER_ATA

#define AUDIO_DEFAULT_SAMPLE_RATE 48000
#define AUDIO_DEFAULT_LATENCY 100

//#define DEBUG_BLASTER
//#define DEBUG_DMA

//#define BENCHMARK_BIOS

// -----------------------------------------------
// End of configuration block, do not modify these
// -----------------------------------------------

#ifdef DO_NOT_FORCE_UNREACHABLE
extern void UNREACHABLE_FATAL_ERROR ( void );
#endif

#ifdef _WIN32
#	define	DIRSEP_STR	"\\"
#	define	DIRSEP_CHR	'\\'
#else
#	define	DIRSEP_STR	"/"
#	define	DIRSEP_CHR	'/'
#endif

#ifdef __GNUC__
#	define LIKELY(__x__)		__builtin_expect(!!(__x__), 1)
#	define UNLIKELY(__x__)		__builtin_expect(!!(__x__), 0)
#	ifdef DO_NOT_FORCE_UNREACHABLE
#		define UNREACHABLE()	UNREACHABLE_FATAL_ERROR()
#	else
#		define UNREACHABLE()	__builtin_unreachable()
#	endif
#	ifdef DO_NOT_FORCE_INLINE
#		define INLINE		inline
#	else
#		define INLINE		__attribute__ ((__always_inline__)) inline
#	endif
#else
#	define LIKELY(__x__)	(__x__)
#	define UNLIKELY(__x__)	(__x__)
#	define INLINE		inline
#	define UNREACHABLE()	UNREACHABLE_FATAL_ERROR()
#endif

#if defined(USE_KVM) && !defined(__linux__)
#undef USE_KVM
#endif

#endif
