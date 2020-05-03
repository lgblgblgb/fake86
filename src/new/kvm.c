#define _DEFAULT_SOURCE

#define USE_KVM

#ifdef USE_KVM

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/kvm.h>

#define MEMORY_SIZE	0x110000



struct kvm {
	int	kvm_fd;
	int	vm_fd;
	int	vcpu_fd;
	uint8_t	*mem;
	struct kvm_run	 *kvm_run;
	struct kvm_sregs sregs;
	struct kvm_regs  regs;
};

static struct kvm kvm = {
	.kvm_fd  = -1,
	.vm_fd   = -1,
	.vcpu_fd = -1,
	.mem     = MAP_FAILED
};



static void get_regs ( void )
{
        if (ioctl(kvm.vcpu_fd, KVM_GET_SREGS, &kvm.sregs) < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}
	if (ioctl(kvm.vcpu_fd, KVM_GET_REGS, &kvm.regs) < 0) {
		perror("KVM_GET_REGS");
		exit(1);
	}
}


static void set_regs ( void )
{
        if (ioctl(kvm.vcpu_fd, KVM_SET_SREGS, &kvm.sregs) < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}
	if (ioctl(kvm.vcpu_fd, KVM_SET_REGS, &kvm.regs) < 0) {
		perror("KVM_SET_REGS");
		exit(1);
	}
}


static void kvm_uninit ( void )
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
		munmap(kvm.mem, MEMORY_SIZE);
		kvm.mem = MAP_FAILED;
	}
}


static int kvm_init ( size_t mem_size )
{
	if (kvm.kvm_fd >= 0 || kvm.vm_fd >= 0 || kvm.vcpu_fd >= 0 || kvm.mem != MAP_FAILED) {
		fprintf(stderr, "KVM: already intiailized, tried twice?\n");
		return 1;
	}
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
	printf("KVM: API version: %d\n", api_ver);
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
	return 0;
error:
	kvm_uninit();
	return 1;
}



int kvm_run ( void )
{
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
}



static void setup_test( void )
{
	get_regs();


	printf("Testing real mode\n");

        //if (ioctl(kvm.vcpu_fd, KVM_GET_SREGS, &kvm.sregs) < 0) {
	//	perror("KVM_GET_SREGS");
	//	exit(1);
	//}

	kvm.sregs.cs.selector = 0;
	kvm.sregs.ds.selector = 0;
	kvm.sregs.es.selector = 0;
	kvm.sregs.ss.selector = 0;
	kvm.sregs.cs.base = 0;
	kvm.sregs.ds.base = 0;
	kvm.sregs.es.base = 0;
	kvm.sregs.ss.base = 0;

        //if (ioctl(kvm.vcpu_fd, KVM_SET_SREGS, &kvm.sregs) < 0) {
	//	perror("KVM_SET_SREGS");
	//	exit(1);
	//}

	memset(&kvm.regs, 0, sizeof(kvm.regs));
	/* Clear all FLAGS bits, except bit 1 which is always set. */
	kvm.regs.rflags = 2;
	kvm.regs.rip = 0x7C00;
	kvm.regs.rsp = 0x400;

	static const uint8_t instseq[] = { 0xb8, 0x76, 0x19, 0x8e, 0xc0, 0xf4 };

	memcpy(kvm.mem + 0x7C00, instseq, sizeof(instseq));

	//if (ioctl(kvm.vcpu_fd, KVM_SET_REGS, &kvm.regs) < 0) {
	//	perror("KVM_SET_REGS");
	//	exit(1);
	//}
	set_regs();

	//memcpy(vm->mem, guest16, guest16_end-guest16);
}
	




int main () {
	kvm_init(MEMORY_SIZE);
	setup_test();
	kvm_run();
}

#endif
