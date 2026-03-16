# PALLOC

PALLOC is a kernel-level memory allocator that exploits page-based virtual-to-physical memory translation to selectively allocate memory pages of each application to the desired DRAM banks. The goal of PALLOC is to control applications' memory locations in a way to minimize memory performance unpredictability in multicore systems by eliminating bank sharing among applications executing in parallel. PALLOC is a software based solution, which is fully compatible with existing COTS hardware platforms and transparent to applications (i.e., no need to modify application code.)

## Source code

The source code of the Linux 3.6.0 kernel with PALLOC support can be obtained as follows.

    $ git clone --depth 1 -b palloc-3.6 https://github.com/heechul/linux.git

Or you can use one of the prepared patches for different Linux kernel versions.

To build the kernel with PALLOC enabled, the following option must be enabled.

    CONFIG_CGROUP_PALLOC=y

## Detecting DRAM bank bits (for DRAM bank partitioning)

See [README-map-detector.md](./README-map-detector.md)

For cache partitioning, just use the cache set bits instead of DRAM bank bits.

## Usage

1. Prepare a phsyical address to DRAM bank mapping file (e.g., map.txt outfrom from drama-pp)
   - Direct address maps (e.g., Raspberry Pi 4)
   ```
    # cat map.txt
    12
    13
    14
	# cat map.txt > /sys/kernel/debug/palloc/control
    --> select bit 12, 13, 14. (total banks: 2^3 = 8)
   ```
   - XOR address maps (e.g., Intel Skylake) 
   ```
	# cat map.txt
	14 18
	15 19
	16 20
	17 21
	8 9 12 13 14 15
    # cat map.txt > /sys/kernel/debug/palloc/control
	--> select (14 XOR 18), (15 XOR 19), (16 XOR 20), and (17 XOR 21) and (8 XOR 9 XOR 12 XOR 13 XOR 14 XOR 15) (total bins: 2^5 = 32)
   ```      
2. CGROUP partition setting
   ```
    # echo "+palloc" > /sys/fs/cgroup/cgroup.subtree_control
	# mkdir /sys/fs/cgroup/part1
	# echo 0 > /sys/fs/cgroup/part1/cpuset.cpus
	# echo 0 > /sys/fs/cgroup/part1/cpuset.mems
	# echo 0-3 > /sys/fs/cgroup/part1/palloc.bins
	--> bin 0,1,2,3 are assigned to part1 CGROUP.
	# echo $$ > /sys/fs/cgroup/part1/tasks
	--> from now on, all processes invoked from the shell use pages from the bins 0,1,2,3 only.
   ```
3. Enable PALLOC
   ```
	# echo 1 > /sys/kernel/debug/palloc/use_palloc
	--> enable palloc (owise the default buddy allocator will be used)
	# echo 1 > /sys/kernel/debug/palloc/debug_level  
	--> enable debug messsages visible through /sys/kernel/debug/tracing/trace. [Recommended]
	# echo 4 > /sys/kernel/debug/palloc/alloc_balance
	--> wait until at least 4 different colors are in the color cache. [Recommended]
	# echo never > /sys/kernel/mm/transparent_hugepage/enabled
	--> palloc doesn't work with transparent huge page. please disable this. [Recommended]
   ```
	 
## Papers

* Heechul Yun, Renato, Zheng-Pei Wu, Rodolfo Pellizzoni. "PALLOC: DRAM Bank-Aware Memory Allocator for Performance Isolation on Multicore Platforms," _IEEE Intl. Conference on Real-Time and Embedded Technology and Applications Symposium (RTAS)_, 2014. ([pdf](http://www.ittc.ku.edu/~heechul/papers/palloc-rtas2014.pdf), [ppt](http://www.slideshare.net/saiparan/palloc-rtas2014))
