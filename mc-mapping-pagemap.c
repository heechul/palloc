/**
 * DRAM controller address mapping detector
 *
 * Copyright (C) 2013  Heechul Yun <heechul@illinois.edu> 
 * Copyright (C) 2018  Heechul Yun <heechul@ku.edu> 
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
#include <inttypes.h>
#include <pthread.h>

/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define MAX_BIT   (24)                  // [27:23] bits are used for iterations
#define CACHE_LINE_SIZE 64

#define MAX(a,b) ((a>b)?(a):(b))
#define MIN(a,b) ((a>b)?(b):(a))
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
int g_cache_num_ways = 16;
void *g_mapping;
int g_cpuid = 0;
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
	g_mem_size =
		(long)(g_fraction_of_physical_memory * getPhysicalMemorySize());
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
ulong  getPhysicalAddr(ulong virtual_addr)
{
	u_int64_t value;
	off_t offset = (virtual_addr / 4096) * sizeof(value);
	int got = pread(g_pagemap_fd, &value, 8, offset);
	//printf("vaddr=%lu, value=0x%llx, got=%d\n", virtual_addr, value, got);
	assert(got == 8);

	// Check the "page present" flag.
	assert(value & (1ULL << 63));

	ulong frame_num = frameNumberFromPagemap(value);
	return (frame_num * 4096) | (virtual_addr & (4095));
}

// ----------------------------------------------
void initPagemap()
{
	g_pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
	assert(g_pagemap_fd >= 0);
}

// ----------------------------------------------
long utime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	return (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
}

uint64_t nstime()
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return ts.tv_sec * 1000000000 + ts.tv_nsec;
}

/**************************************************************************
 * Implementation
 **************************************************************************/

long *create_list(ulong match_mask, int max_shift, int min_count)
{
	ulong vaddr, paddr;
	int count = 0;
	long *list_curr = NULL;
	long *list_head = NULL;
	
	// printf("mask: 0x%lx, shift: %d\n", match_mask, max_shift);
	
	for (int i = 0; i < g_mem_size; i += 0x1000) {
		vaddr = (ulong)(g_mapping + i) + (match_mask & 0xFFF);
		if (*(ulong *)vaddr > 0)
			continue;
		paddr = getPhysicalAddr(vaddr);
		if (!((paddr & ((1<<max_shift) - 1)) ^ match_mask)) {
			/* found a match */
			// printf("vaddr-paddr: %p-%p\n", (void *)vaddr, (void *)paddr);
			count ++;
			
			if (count == 1) {
				list_head = list_curr = (long *)vaddr;
			}

			*list_curr = vaddr;
			list_curr = (long *)vaddr;
				
			if (count == min_count) {
				*list_curr = (ulong) list_head;
				// printf("#of entries in the list: %d\n", count);
				return list_head;
			}
		}
	}
	printf("failed: found (%d) / requested (%d) pages\n", count, min_count);
	return NULL;
}

int run(long *list, int count)
{
	int i = 0;
	while (list && i++ < count) {
		list = (long *)*list;
	}
	return i;
}

void  worker(void *param)
{
	long *list = (long *)param;

	printf("worker thread begins\n");

	while (list) {
		list = (long *)*list;
	}
}

int main(int argc, char* argv[])
{
	struct sched_param param;
        cpu_set_t cmask;
	int num_processors, n_corun = 1;
	int opt;
	int repeat = 1000000;
	
	pthread_t tid[16]; /* thread identifier */
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	num_processors = sysconf(_SC_NPROCESSORS_CONF);

	/*
	 * get command line options 
	 */
	while ((opt = getopt(argc, argv, "w:p:c:n:i:h")) != -1) {
		switch (opt) {
		case 'w': /* cache num ways */
			g_cache_num_ways = strtol(optarg, NULL, 0);
			break;
		case 'p': /* set memory fraction */
			g_fraction_of_physical_memory = strtof(optarg, NULL);
			break;
		case 'c': /* set CPU affinity */
			g_cpuid = strtol(optarg, NULL, 0);
			break;
		case 'n': /* #of co-runners */
			n_corun = strtol(optarg, NULL, 0);
			break;
		case 'i': /* iterations */
			repeat = strtol(optarg, NULL, 0);
			printf("repeat=%d\n", repeat);
			break;
		}

	}
	
	initPagemap();
	setupMapping();
	
	/* try to use a real-time scheduler*/
	param.sched_priority = 10;
	if(sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
		perror("sched_setscheduler failed");
	}

	/* launch corun worker threads */
	tid[0]= pthread_self();
	long *corun_list[16];

	/* thread affinity set */
	for (int i = 0; i < MIN(1+n_corun, num_processors); i++) {
		if (i != 0) {
			corun_list[i] = create_list(0x0, MAX_BIT, g_cache_num_ways*2);
			pthread_create(&tid[i], &attr, (void *)worker, corun_list[i]);
		}
		CPU_ZERO(&cmask);
		CPU_SET((g_cpuid + i) % num_processors, &cmask);
		if (pthread_setaffinity_np(tid[i], sizeof(cpu_set_t), &cmask) < 0)
			perror("error");
	}

	sleep(5);

	for (int bit = 6; bit < 24; bit++){
		/* initialize data */
		ulong bank_mask = (1<<bit);
		long *subject_list =
			create_list(bank_mask, MAX_BIT, g_cache_num_ways*2);

		/* subject measurement */
		struct timespec start, end;
		clock_gettime(CLOCK_REALTIME, &start);
		
		int naccess = run(subject_list, repeat);

		clock_gettime(CLOCK_REALTIME, &end);
		int64_t nsdiff = get_elapsed(&start, &end);

		/* double  avglat = (double)nsdiff/naccess; */
		/* printf("size: %d MB\n", (int)(g_mem_size/1024/1024)); */
		/* printf("duration %"PRId64"ns, #access %d\n", nsdiff, naccess); */
		/* printf("average latency: %.2f ns\n", avglat); */
		printf("Bit%d: %.2f MB/s, %.2f ns\n", bit,
		       (double)64*1000*naccess/nsdiff,
		       (double)nsdiff/naccess);
	}

	return 0;
} 
