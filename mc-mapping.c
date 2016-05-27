/**
 * 
 *
 * Copyright (C) 2013  Heechul Yun <heechul@illinois.edu> 
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 */ 

/**************************************************************************
 * Conditional Compilation Options
 **************************************************************************/

/**************************************************************************
 * Included Files
 **************************************************************************/
#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <assert.h>

/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define L3_NUM_WAYS   16                    // cat /sys/devices/system/cpu/cpu0/cache/index3/ways..
#define NUM_ENTRIES   (L3_NUM_WAYS*2)       // # of list entries to iterate
#define ENTRY_SHIFT   (24)                  // [27:23] bits are used for iterations
#define ENTRY_DIST    (1<<ENTRY_SHIFT)      // distance between the two entries
#define CACHE_LINE_SIZE 64

#define MAX(a,b) ((a>b)?(a):(b))
#define CEIL(val,unit) (((val + unit - 1)/unit)*unit)

#define FATAL do { fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", \
   __LINE__, __FILE__, errno, strerror(errno)); exit(1); } while(0)

/**************************************************************************
 * Public Types
 **************************************************************************/

/**************************************************************************
 * Global Variables
 **************************************************************************/
static int g_mem_size = NUM_ENTRIES * ENTRY_DIST;
static int* list;
static int next;

/**************************************************************************
 * Public Function Prototypes
 **************************************************************************/
uint64_t get_elapsed(struct timespec *start, struct timespec *end)
{
	uint64_t dur;
	if (start->tv_nsec > end->tv_nsec)
		dur = (uint64_t)(end->tv_sec - 1 - start->tv_sec) * 1000000000 +
			(1000000000 + end->tv_nsec - start->tv_nsec);
	else
		dur = (uint64_t)(end->tv_sec - start->tv_sec) * 1000000000 +
			(end->tv_nsec - start->tv_nsec);

	return dur;

}

/**************************************************************************
 * Implementation
 **************************************************************************/
int run(int iter)
{
	int i;
	int cnt = 0;
	for (i = 0; i < iter; i++) {
		next = list[next];
		cnt ++;
	}
	return cnt;
}


int main(int argc, char* argv[])
{
	struct sched_param param;
        cpu_set_t cmask;
	int num_processors;
	int cpuid = 0;
	int use_dev_mem = 0;

	int *memchunk = NULL;
	int opt, prio;
	int i;

	int repeat = 1000;

	int page_shift = 0;
	int xor_page_shift = 0;

	/*
	 * get command line options 
	 */
	while ((opt = getopt(argc, argv, "a:xb:s:o:m:c:i:l:h")) != -1) {
		switch (opt) {
		case 'b': /* bank bit */
			page_shift = strtol(optarg, NULL, 0);
			break;
		case 's': /* xor-bank bit */
			xor_page_shift = strtol(optarg, NULL, 0);
			break;
		case 'm': /* set memory size */
			g_mem_size = 1024 * strtol(optarg, NULL, 0);
			break;
		case 'x': /* mmap to /dev/mem, owise use hugepage */
			use_dev_mem = 1;
			break;
		case 'c': /* set CPU affinity */
			cpuid = strtol(optarg, NULL, 0);
			num_processors = sysconf(_SC_NPROCESSORS_CONF);
			CPU_ZERO(&cmask);
			CPU_SET(cpuid % num_processors, &cmask);
			if (sched_setaffinity(0, num_processors, &cmask) < 0)
				perror("error");
			break;
		case 'p': /* set priority */
			prio = strtol(optarg, NULL, 0);
			if (setpriority(PRIO_PROCESS, 0, prio) < 0)
				perror("error");
			break;
		case 'i': /* iterations */
			repeat = strtol(optarg, NULL, 0);
			printf("repeat=%d\n", repeat);
			break;
		}

	}

	g_mem_size += (1 << page_shift);
	g_mem_size = CEIL(g_mem_size, ENTRY_DIST);

	/* alloc memory. align to a page boundary */
	if (use_dev_mem) {
		int fd = open("/dev/mem", O_RDWR | O_SYNC);
		void *addr = (void *) 0x1000000080000000;


		if (fd < 0) {
			perror("Open failed");
			exit(1);
		}
		
		memchunk = mmap(0, g_mem_size,
				PROT_READ | PROT_WRITE, 
				MAP_SHARED, 
				fd, (off_t)addr);
	} else {
		memchunk = mmap(0, g_mem_size,
				PROT_READ | PROT_WRITE, 
				MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, 
				-1, 0);
	}

	if (memchunk == MAP_FAILED) {
		perror("failed to alloc");
		exit(1);
	}

	/* initialize data */
	int off_idx = (1<<page_shift) / 4;
	
	if (xor_page_shift > 0) {
		off_idx = ((1<<page_shift) + (1<<xor_page_shift)) / 4;
	}

#if 0
	if (page_shift > 0 || xor_page_shift > 0)
		off_idx ++;
#else
	if (page_shift >= ENTRY_SHIFT || xor_page_shift >= ENTRY_SHIFT) {
		fprintf(stderr, "page_shift or xor_page_shift must be less than %d bits\n",
			ENTRY_SHIFT);
		exit(1);
	}
#endif

	list = &memchunk[off_idx];
	for (i = 0; i < NUM_ENTRIES; i++) {
		int idx = i * ENTRY_DIST / 4;
		if (i == (NUM_ENTRIES - 1))
			list[idx] = 0;
		else
			list[idx] = (i+1) * ENTRY_DIST/4;
	}
	next = 0;
	printf("pshift: %d, XOR-pshift: %d\n", page_shift, xor_page_shift);

#if 1
        param.sched_priority = 10;
        if(sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
		perror("sched_setscheduler failed");
        }
#endif
	struct timespec start, end;

	clock_gettime(CLOCK_REALTIME, &start);

	/* actual access */
	int naccess = run(repeat);

	clock_gettime(CLOCK_REALTIME, &end);

	int64_t nsdiff = get_elapsed(&start, &end);
	double  avglat = (double)nsdiff/naccess;

	printf("size: %d (%d KB)\n", g_mem_size, g_mem_size/1024);
	printf("duration %"PRId64"ns, #access %d\n", nsdiff, naccess);
	printf("average latency: %.2f ns\n", avglat);
	printf("bandwidth %.2f MB/s\n", (double)64*1000*naccess/nsdiff);

	return 0;
}
