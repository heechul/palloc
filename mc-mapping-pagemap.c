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
#include <sys/sysinfo.h>

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
long g_mem_size;
double g_fraction_of_physical_memory = 0.2;
int g_cache_num_ways = 20;
void *g_mapping;
long *g_list;

int g_pagemap_fd;

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

// ----------------------------------------------
size_t getPhysicalMemorySize() {
	struct sysinfo info;
	sysinfo(&info);
	return (size_t) info.totalram * (size_t) info.mem_unit;
}

// ----------------------------------------------
void setupMapping() {
	g_mem_size = (long)((double)getPhysicalMemorySize() * g_fraction_of_physical_memory);

	printf("mem_size (MB): %d\n", (int)(g_mem_size / 1024 / 1024));
	
	/* map */
	g_mapping = mmap(NULL, g_mem_size, PROT_READ | PROT_WRITE,
		       MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	assert(g_mapping != (void *) -1);

	/* initialize */
	for (int index = 0; index < g_mem_size; index += 0x1000) {
		char *byte = (char *)g_mapping + index;
		byte[0] = index % 256;
	}

	printf("allocation complete.\n");
}

// ----------------------------------------------
size_t frameNumberFromPagemap(size_t value) {
	return value & ((1ULL << 54) - 1);
}

// ----------------------------------------------
ulong  getPhysicalAddr(ulong virtual_addr) {
	ulong value;
	off_t offset = (virtual_addr / 4096) * sizeof(value);
	int got = pread(g_pagemap_fd, &value, sizeof(value), offset);
	assert(got == 8);

	// Check the "page present" flag.
	assert(value & (1ULL << 63));

	ulong frame_num = frameNumberFromPagemap(value);
	return (frame_num * 4096) | (virtual_addr & (4095));
}

// ----------------------------------------------
void initPagemap() {
	g_pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
	assert(g_pagemap_fd >= 0);
}

// ----------------------------------------------
long utime() {
	struct timeval tv;
	gettimeofday(&tv, NULL);

	return (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
}

/**************************************************************************
 * Implementation
 **************************************************************************/
long *create_list(ulong match_mask, int max_shift)
{
	ulong vaddr, paddr;
	int count = 0;
	long *list_curr = NULL;

	printf("mask: 0x%lx, shift: %d\n", match_mask, max_shift);
	
	for (int i = 0; i < g_mem_size; i += 0x1000) {
		vaddr = (ulong)(g_mapping + i);
		paddr = getPhysicalAddr(vaddr);
		if (!((paddr & ((1<<max_shift) - 1)) ^ match_mask)) {
			/* found a match */
			printf("vaddr-paddr: %p-%p\n", (void *)vaddr, (void *)paddr);		
			
			if (count == 0) {
				g_list = list_curr = (long *)vaddr;
			} else {
				*list_curr = vaddr;
				list_curr = (long *)vaddr;
				if (count == g_cache_num_ways) {
					*list_curr = (ulong)g_list;
					printf("#of entries in the list: %d\n", ++count);
					return g_list;
				}
			}
			count ++;
		}
	}
	printf("failed to find matching pages\n");
	return NULL;
}

int run(long *list, int count)
{
	int i;
	while (list && i++ < count) {
		list = (long *)*list;
	}
	return i;
}

int main(int argc, char* argv[])
{
	struct sched_param param;
        cpu_set_t cmask;
	int num_processors;
	int cpuid = 0;

	int opt;

	int repeat = 10000;

	int page_shift = 0;
	int xor_page_shift = 0;

	ulong bank_mask = 0x0;
	
	/*
	 * get command line options 
	 */
	while ((opt = getopt(argc, argv, "b:s:w:p:c:i:h")) != -1) {
		switch (opt) {
		case 'b': /* bank bit */
			page_shift = strtol(optarg, NULL, 0);
			break;
		case 's': /* xor-bank bit */
			xor_page_shift = strtol(optarg, NULL, 0);
			break;
		case 'w': /* cache num ways */
			g_cache_num_ways = strtol(optarg, NULL, 0);
			break;
		case 'p': /* set memory fraction */
			g_fraction_of_physical_memory = strtof(optarg, NULL);
			break;
		case 'c': /* set CPU affinity */
			cpuid = strtol(optarg, NULL, 0);
			num_processors = sysconf(_SC_NPROCESSORS_CONF);
			CPU_ZERO(&cmask);
			CPU_SET(cpuid % num_processors, &cmask);
			if (sched_setaffinity(0, num_processors, &cmask) < 0)
				perror("error");
			break;
		case 'i': /* iterations */
			repeat = strtol(optarg, NULL, 0);
			printf("repeat=%d\n", repeat);
			break;
		}

	}
	
	initPagemap();
	setupMapping();
	
	/* initialize data */
	if (page_shift > 0 )
		bank_mask = (1<<page_shift);
	
	if (xor_page_shift > 0) {
		bank_mask= (1<<page_shift) + (1<<xor_page_shift);
	}

	long *list = create_list(bank_mask, ENTRY_SHIFT);

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
	int naccess = run(list, repeat);

	clock_gettime(CLOCK_REALTIME, &end);

	int64_t nsdiff = get_elapsed(&start, &end);
	double  avglat = (double)nsdiff/naccess;

	printf("size: %d MB\n", (int)(g_mem_size/1024/1024));
	printf("duration %"PRId64"ns, #access %d\n", nsdiff, naccess);
	printf("average latency: %.2f ns\n", avglat);
	printf("bandwidth %.2f MB/s\n", (double)64*1000*naccess/nsdiff);

	return 0;
} 
