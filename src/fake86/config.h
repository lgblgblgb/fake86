#ifndef __CONFIG_H__
#define __CONFIG_H__

#define BUILD_STRING "Fake86 v0.13.9.16"

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
#endif
