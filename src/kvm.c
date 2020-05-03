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

#include "config.h"

#ifdef USE_KVM

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/kvm.h>

#include "kvm.h"


struct kvm kvm = {
	.kvm_fd  = -1,
	.vm_fd   = -1,
	.vcpu_fd = -1,
	.mem     = MAP_FAILED
};


static int get_regs ( void )
{
        if (ioctl(kvm.vcpu_fd, KVM_GET_SREGS, &kvm.sregs) < 0) {
		perror("KVM: GET_SREGS");
		return 1;
	}
	if (ioctl(kvm.vcpu_fd, KVM_GET_REGS, &kvm.regs) < 0) {
		perror("KVM: GET_REGS");
		return 1;
	}
	return 0;
}


static int set_regs ( void )
{
	kvm.regs.rflags |= 2;	// bit 1 of EFLAGS is always set
        if (ioctl(kvm.vcpu_fd, KVM_SET_SREGS, &kvm.sregs) < 0) {
		perror("KVM: SET_SREGS");
		return 1;
	}
	if (ioctl(kvm.vcpu_fd, KVM_SET_REGS, &kvm.regs) < 0) {
		perror("KVM_SET_REGS");
		return 1;
	}
	return 0;
}


void kvm_uninit ( void )
{
	if (kvm.vcpu_fd >= 0) {
		close(kvm.vcpu_fd);
		kvm.vcpu_fd = -1;
	}
	if (kvm.vm_fd >= 0) {
		close(kvm.vm_fd);
		kvm.vm_fd = -1;
	}
	if (kvm.kvm_fd >= 0) {
		close(kvm.kvm_fd);
		kvm.kvm_fd = -1;
	}
	if (kvm.mem != MAP_FAILED) {
		munmap(kvm.mem, kvm.mem_size);
		kvm.mem = MAP_FAILED;
	}
}


int kvm_init ( size_t mem_size )
{
	if (kvm.kvm_fd >= 0 || kvm.vm_fd >= 0 || kvm.vcpu_fd >= 0 || kvm.mem != MAP_FAILED) {
		fprintf(stderr, "KVM: already intiailized, tried twice?\n");
		return 1;
	}
	kvm.mem_size = mem_size;
	kvm.kvm_fd = open("/dev/kvm", O_RDWR);
	if (kvm.kvm_fd < 0) {
		perror("KVM: Cannot open /dev/kvm");
		goto error;
	}
	int api_ver = ioctl(kvm.kvm_fd, KVM_GET_API_VERSION, 0);
	if (api_ver < 0) {
		perror("KVM: Cannot query API version");
		goto error;
	}
	if (api_ver != KVM_API_VERSION) {
		fprintf(stderr, "KVM: API version mismatch: got %d but expected %d\n", api_ver, KVM_API_VERSION);
		goto error;
	}
	kvm.vm_fd = ioctl(kvm.kvm_fd, KVM_CREATE_VM, 0);
	if (kvm.vm_fd < 0) {
		perror("KVM: Cannot create VM");
		goto error;
	}
        if (ioctl(kvm.vm_fd, KVM_SET_TSS_ADDR, 0xfffbd000) < 0) {
                perror("KVM: KVM_SET_TSS_ADDR failed");
		goto error;
	}
	kvm.mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
	if (kvm.mem == MAP_FAILED) {
		perror("KVM: mmap() failed");
		goto error;
	}
	//if (madvise(kvm.mem, mem_size, MADV_MERGEABLE))
	//	perror("KVM: madvise() failed");
	struct kvm_userspace_memory_region memreg;
	memreg.slot = 0;
	memreg.flags = 0;
	memreg.guest_phys_addr = 0;
	memreg.memory_size = mem_size;
	memreg.userspace_addr = (unsigned long)kvm.mem;
	//memreg.userspace_addr = (unsigned long)RAM;
        if (ioctl(kvm.vm_fd, KVM_SET_USER_MEMORY_REGION, &memreg) < 0) {
		perror("KVM: cannot configure memory mapping (KVM_SET_USER_MEMORY_REGION)");
                goto error;
	}
	int vcpu_mmap_size;
	kvm.vcpu_fd = ioctl(kvm.vm_fd, KVM_CREATE_VCPU, 0);
        if (kvm.vcpu_fd < 0) {
		perror("KVM: cannot create vCPU (KVM_CREATE_VCPU)");
                goto error;
	}
	vcpu_mmap_size = ioctl(kvm.kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
        if (vcpu_mmap_size <= 0) {
		perror("KVM: KVM_GET_VCPU_MMAP_SIZE");
                goto error;
	}
	kvm.kvm_run = mmap(NULL, vcpu_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, kvm.vcpu_fd, 0);
	if (kvm.kvm_run == MAP_FAILED) {
		perror("KVM: mmap kvm_run");
		goto error;
	}
	if (get_regs())
		return -1;
	kvm.sregs.cs.selector = 0;
	kvm.sregs.ds.selector = 0;
	kvm.sregs.es.selector = 0;
	kvm.sregs.ss.selector = 0;
	kvm.sregs.cs.base = 0;
	kvm.sregs.ds.base = 0;
	kvm.sregs.es.base = 0;
	kvm.sregs.ss.base = 0;
	memset(&kvm.regs, 0, sizeof(kvm.regs));
	if (set_regs())
		return -1;
	printf("KVM: successfully initialized, API version: %d\n", api_ver);
	return 0;
error:
	kvm_uninit();
	return 1;
}



int kvm_run ( void )
{
	if (set_regs())
		return -1;
	printf("KVM: entering @ %Xh:%Xh\n", (unsigned int)kvm.sregs.cs.selector, (unsigned int)kvm.regs.rip);
	if (ioctl(kvm.vcpu_fd, KVM_RUN, 0) < 0) {
		perror("KVM: virtual machine cannot be run");
		return -1;
	}
	if (get_regs())
		return -1;
	printf("KVM: VM exited with code %d at %Xh:%Xh\n", kvm.kvm_run->exit_reason, (unsigned int)kvm.sregs.cs.selector, (unsigned int)kvm.regs.rip);
	return 0;
#if 0

	uint64_t memval = 0;

	for (;;) {
		printf("Entering run!\n");
		if (ioctl(kvm.vcpu_fd, KVM_RUN, 0) < 0) {
			perror("KVM_RUN");
			exit(1);
		}
		printf("Leavning run ...\n");

		switch (kvm.kvm_run->exit_reason) {
		case KVM_EXIT_HLT:
			puts("Exit on HLT.");
			goto check;

		case KVM_EXIT_IO:
			if (kvm.kvm_run->io.direction == KVM_EXIT_IO_OUT
			    && kvm.kvm_run->io.port == 0xE9) {
				char *p = (char *)kvm.kvm_run;
				fwrite(p + kvm.kvm_run->io.data_offset,
				       kvm.kvm_run->io.size, 1, stdout);
				fflush(stdout);
				continue;
			}

			/* fall through */
		default:
			fprintf(stderr,	"Got exit_reason %d,"
				" expected KVM_EXIT_HLT (%d)\n",
				kvm.kvm_run->exit_reason, KVM_EXIT_HLT);
			break;
		}
	}

 check:
	get_regs();
	//if (ioctl(kvm.vcpu_fd, KVM_GET_REGS, &kvm.regs) < 0) {
	//	perror("KVM_GET_REGS");
	//	exit(1);
	//}
	//if (ioctl(kvm.vcpu_fd, KVM_GET_SREGS, &sregs) < 0) {
	//	perror("KVM_GET_SREGS");
	//	exit(1);
	//}
	printf("RIP=%Xh ES/selector=%Xh ES/base=%Xh\n", kvm.regs.rip, kvm.sregs.es.selector, kvm.sregs.es.base);


	return 1;
#endif
}



	



#if 0
int main () {
	kvm_init(MEMORY_SIZE);
	setup_test();
	kvm_run();
}
#endif

#endif
