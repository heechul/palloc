/**
 *
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
#include <assert.h>
#include <linux/kernel-page-flags.h>
#include <stdint.h>
#include <sys/sysinfo.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <vector>
#include <list>
#include <set>

/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define MAX_BIT   (22)                  // [27:23] bits are used for iterations
#define debug(f, ...) do { if(verbosity > 3) {printf("[%-9s] ", "DEBUG"); \
      printf(f, __VA_ARGS__); }} while(0);

/**************************************************************************
 * Global Variables
 **************************************************************************/
long g_mem_size;
double g_fraction_of_physical_memory = 0.2;
int g_cache_num_ways = 16;
int L3_thresh_cycles = 200;

void *g_mapping;
ulong *g_frame_phys;

int g_cpuid = 0;
int g_pagemap_fd;

int verbosity = 4;

size_t num_reads = 500;

using namespace std;

/**************************************************************************
 * Public Function Prototypes
 **************************************************************************/
size_t getPhysicalMemorySize() {
  struct sysinfo info;
  sysinfo(&info);
  return (size_t) info.totalram * (size_t) info.mem_unit;
}

size_t frameNumberFromPagemap(size_t value) {
  return value & ((1ULL << 54) - 1);
}

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

void setupMapping() {
  g_mem_size =
    (long)(g_fraction_of_physical_memory * getPhysicalMemorySize());
  printf("mem_size (MB): %d\n", (int)(g_mem_size / 1024 / 1024));
	
  /* map */
  g_mapping = mmap(NULL, g_mem_size, PROT_READ | PROT_WRITE,
                   MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  assert(g_mapping != (void *) -1);
  
  /* page virt -> phys translation table */
  g_frame_phys = (ulong *)malloc(sizeof(long) * (g_mem_size / 0x1000));
	
  /* initialize */
  for (long i = 0; i < g_mem_size; i += 0x1000) {
    ulong vaddr, paddr;
    vaddr = (ulong)((ulong)g_mapping + i);
    *((ulong *)vaddr) = 0x0;
    paddr = getPhysicalAddr(vaddr);
    g_frame_phys[i/0x1000] = paddr;
    // printf("vaddr-paddr: %p-%p\n", (void *)vaddr, (void *)paddr);
  }
  printf("allocation complete.\n");
}

void initPagemap()
{
  g_pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
  assert(g_pagemap_fd >= 0);
}

uint64_t rdtsc() {
  uint64_t a, d;
  asm volatile ("xor %%rax, %%rax\n" "cpuid"::: "rax", "rbx", "rcx", "rdx");
  asm volatile ("rdtscp" : "=a" (a), "=d" (d) : : "rcx");
  a = (d << 32) | a;
  return a;
}

uint64_t rdtsc2() {
  uint64_t a, d;
  asm volatile ("rdtscp" : "=a" (a), "=d" (d) : : "rcx");
  asm volatile ("cpuid"::: "rax", "rbx", "rcx", "rdx");
  a = (d << 32) | a;
  return a;
}

int run(long *list, int count)
{
	long i = 0;
	while (list && i++ < count) {
		list = (long *)*list;
	}
	return i;
}

bool check_conflict(void *addr, set<void *> &EV)
{
  uint64_t sum = 0;
  long *list_curr = NULL;
  long *list_head = NULL;
  int count = 0;
  for (void *vaddr: EV) {
    count ++;
    if (count == 1) {
      list_head = list_curr = (long *)vaddr;
    }
    *list_curr = (long)vaddr;
    list_curr = (long *)vaddr;

    if (count == (int)EV.size()) {
      *list_curr = (ulong) list_head;
    }
  }    

  size_t t0 = rdtsc();
  sum = run(list_head, num_reads);
  uint64_t res = (rdtsc2() - t0) / (num_reads);
  printf("took: %d cycles/iteration. sum=%ld\n", (int)res, (long)sum);
  if ((int)res > L3_thresh_cycles)
    return true;
  else
    return false;
}

bool find_EV(set<void *> &CS, set<void *> &EV)
{
  set<void *>::iterator it;
  set<void *> CS2;

  EV.clear();
  CS2 = CS;
  it = CS2.begin();
  void *test_addr = *it;
  CS2.erase(it);

  printf("pass 1\n");  
  if (check_conflict(test_addr, CS2) == false) {
    return false;
  }

  printf("pass 2\n");
  
  for (it = CS2.begin(); it != CS2.end(); it++) {
    void *addr = *it;
    printf("%p ", addr);
    set<void *> tmpS = CS2;
    tmpS.erase(addr); // tmpS = CS2 - addr
    if (check_conflict(test_addr, tmpS) == true) {
      printf("conflict. add to EV\n");
      EV.insert(addr);
    } else {
      printf("no conflict. continue\n");
      CS2.erase(it);
    }
  }

  printf("pass 3\n");
  for (void *addr: CS) {
    if (check_conflict(test_addr, EV) == true) {
      EV.insert(addr);
    }
  }
  return true;
}

bool find_addresses(ulong match_mask, int max_shift, int min_count, set<void *> &CS)
{
  ulong vaddr, paddr;
  
  for (long i = 0; i < g_mem_size; i += 0x1000) {
    vaddr = (ulong)((long)g_mapping + i) + (match_mask & 0xFFF);
    paddr = g_frame_phys[i/0x1000] + (match_mask & 0xFFF);
    if (!((paddr & ((1<<max_shift) - 1)) ^ match_mask)) {
      if (*(ulong *)vaddr > 0)
        continue;
      /* found a match */
      // printf("vaddr-paddr: %p-%p\n", (void *)vaddr, (void *)paddr);
      CS.insert((void *)vaddr);
      
      if ((int)CS.size() == min_count) {
        return true;
      }
    }
  }
  debug("failed: found (%d) / requested (%d) pages\n", (int)CS.size(), min_count);
  return false;
}

int main(int argc, char *argv[])
{
  set<void*> CS;
  set<void*> EV;
  
  initPagemap();
  setupMapping();

  find_addresses(0x0, MAX_BIT, atoi(argv[1]), CS);
  printf("created a list with %lu addresses\n", CS.size());
  
  /* for (void *addr: CS) { */
  /*   printf("0x%p\n", addr); */
  /* } */
  
  find_EV(CS, EV);

  printf("EV (%d):\n", (int)EV.size());
  for (void *addr: EV) {
    printf("0x%p,", addr);
  }
  printf("\n");
  return 0;
}
