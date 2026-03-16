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

1. Select physical adddress bits to be used for page coloring

   - For normal address bits (e.g., Intel Nehalem)
   ```
	# echo 0x00183000 > /sys/kernel/debug/palloc/palloc_mask
        --> select bit 12, 13, 19, 20. (total bins: 2^4 = 16)
   ```
   - For XOR mapped address bits (e.g., Intel Haswell) 
   ```
	# echo 0x0001e000 > /sys/kernel/debug/palloc/palloc_mask
	# echo xor 13 17 > /sys/kernel/debug/palloc/control
	# echo xor 14 18 > /sys/kernel/debug/palloc/control
	# echo xor 15 19 > /sys/kernel/debug/palloc/control
	# echo xor 16 20 > /sys/kernel/debug/palloc/control
	# echo 1 > /sys/kernel/debug/palloc/use_mc_xor
	--> select (13 XOR 17), (14 XOR 18), (15 XOR 19), and (16 XOR 20) (total bins: 2^4 = 16)
   ```      
   - CGROUP partition setting
   ```
	# mount -t cgroup xxx /sys/fs/cgroup
	# mkdir /sys/fs/cgroup/part1
	# echo 0 > /sys/fs/cgroup/part1/cpuset.cpus
	# echo 0 > /sys/fs/cgroup/part1/cpuset.mems
	# echo 0-3 > /sys/fs/cgroup/part1/palloc.bins
	--> bin 0,1,2,3 are assigned to part1 CGROUP.
	# echo $$ > /sys/fs/cgroup/part1/tasks
	--> from now on, all processes invoked from the shell use pages from the bins 0,1,2,3 only.
   ```
   - Enable PALLOC
   ```
	# echo 1 > /sys/kernel/debug/palloc/use_palloc
	--> enable palloc (owise the default buddy allocator will be used)
   ```
   - Other options
   ```
	# echo 1 > /sys/kernel/debug/palloc/debug_level  
	--> enable debug messsages visible through /sys/kernel/debug/tracing/trace. [Recommended]
	# echo 4 > /sys/kernel/debug/palloc/alloc_balance
	 --> wait until at least 4 different colors are in the color cache. [Recommended]
   ```
2. Disable support for transparent huge pages from kernel:
   ```
	# echo never > /sys/kernel/mm/transparent_hugepage/enabled
	--> palloc doesn't work with transparent huge page. please disable this.
   ```
	 
## Papers

* Heechul Yun, Renato, Zheng-Pei Wu, Rodolfo Pellizzoni. "PALLOC: DRAM Bank-Aware Memory Allocator for Performance Isolation on Multicore Platforms," _IEEE Intl. Conference on Real-Time and Embedded Technology and Applications Symposium (RTAS)_, 2014. ([pdf](http://www.ittc.ku.edu/~heechul/papers/palloc-rtas2014.pdf), [ppt](http://www.slideshare.net/saiparan/palloc-rtas2014))
