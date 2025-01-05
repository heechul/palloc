DRAM Controller Address Map detector
====================================

First, build the detector as follows.

	$ make mc-mapping

Next, run the following script to identify candidate bank bits.
It tests from bit 6 to 29 and for each bit, it reports the measured 
average bandwidth of the microbenchmark (mc-mapping). If successful, 
bits can be categorized into two distinct subgroups. For example, 
the following is the output on an Intel Xeon W3530 (nehalem) machine 
with 1ch 4GB DDR3 DRAM (total 16 banks=2 ranks x 8 banks/rank), which 
was used in our RTAS'14 paper [1]. 

	$ sudo ./detect-mc-mapping.sh
	mc-mapping: no process found
	Run a background task on core1-3
	Now run the test
	Bit6: 293.31
	Bit7: 293.38
	Bit8: 294.12
	Bit9: 293.47
	Bit10: 293.42
	Bit11: 293.39
	Bit12: 780.64		<--- faster
	Bit13: 783.43		<--- faster
	Bit14: 293.37
	Bit15: 293.51
	Bit16: 293.33
	Bit17: 293.53
	Bit18: 293.32
	Bit19: 785.48		<--- faster
	Bit20: 787.71		<--- faster


Notice that bit 12,13,19, and 20 are noticably different
from the other bits. Since we already know from the DRAM 
specification that there are 16 banks, we can conclude that the 
identified 4 bits are used to address the DRAM banks.

!!!WARNING!!! Running 'detect-mc-mapping.sh' can cause system instability or 
even crash because it directly writes through /dev/mem. Therefore, it is 
recommended to reboot the machine after running the script.

## Handling XOR addressing 

If the number of identified bits are more than expected, then it is likely that
the memory controller uses XOR addressing [2].

For example, the following is the output on an Intel Xeon E3-1230 (Haswell) 
machine with 1ch 4GB DDR3 DRAM (total 16 banks=2 ranks x 8 banks/rank). 
In this case, there are total 8 bits (bit 13,14,15,16,17,18,19) that show 
better bandwidth numbers, although only 4 bits are expected. This is because
the memory controller uses XOR address mapping.

	$ sudo ./detect-mc-mapping.sh
	  mc-mapping: no process found
	  Run a background task on core1-3
	  Now run the test
	  Bit6: 347.57
	  Bit7: 337.46
	  Bit8: 335.15
	  Bit9: 339.78
	  Bit10: 344.88
	  Bit11: 337.86
	  Bit12: 337.51
	  Bit13: 629.86		<--- faster
	  Bit14: 613.87		<--- faster
	  Bit15: 617.76		<--- faster
	  Bit16: 608.52		<--- faster
	  Bit17: 630.11		<--- faster
	  Bit18: 628.36		<--- faster
	  Bit19: 631.34		<--- faster
	  Bit20: 628.51		<--- faster
	  Bit21: 314.77
	  Bit22: 315.19
	  Bit23: 314.81
	  Bit24: 309.67
	  Bit25: 314.77
	  Bit26: 315.27
	  Bit27: 315.75
	  Bit28: 315.51
	  Bit29: 310.42

In case the XOR addressing is used, we need to identify which pairs of two bits 
are XOR gated. We provide two scripts to aid this 
identification process. The following is performed on the same E3-1230 
platform. It tests all pairs of two bits out of the total 8 bits. Again, the 
output would form two distinct groups. In this case, the lower bandwidth 
number means that the two bits are XOR paired. 

	 
	$ sudo bash
	# ./gen_combination.py 13 14 15 16 17 18 19 20 | \
		./detect-mc-mapping-xor.sh
	mc-mapping: no process found
	Run a background task on core1-3
	Now run the test
	Bit 13 <--> 14: 613.18
	Bit 13 <--> 15: 631.86
	Bit 13 <--> 16: 608.09
	Bit 13 <--> 17: 316.41  <-- XOR pair
	Bit 13 <--> 18: 629.86
	Bit 13 <--> 19: 629.47
	Bit 13 <--> 20: 629.84
	Bit 14 <--> 15: 629.40
	Bit 14 <--> 16: 629.84
	Bit 14 <--> 17: 628.44
	Bit 14 <--> 18: 315.18  <-- XOR pair
	Bit 14 <--> 19: 610.55
	Bit 14 <--> 20: 629.39
	Bit 15 <--> 16: 628.33
	Bit 15 <--> 17: 631.25
	Bit 15 <--> 18: 628.57
	Bit 15 <--> 19: 315.76  <-- XOR pair
	Bit 15 <--> 20: 630.71
	Bit 16 <--> 17: 628.28
	Bit 16 <--> 18: 630.09
	Bit 16 <--> 19: 628.60
	Bit 16 <--> 20: 310.28  <-- XOR pair
	Bit 17 <--> 18: 629.57
	Bit 17 <--> 19: 631.22
	Bit 17 <--> 20: 628.41
	Bit 18 <--> 19: 630.48
	Bit 18 <--> 20: 615.39
	Bit 19 <--> 20: 617.36

Hence, we can conclude the final mappings, which select DRAM banks, are
(13 XOR 17), (14 XOR 18), (15 XOR 19), and (16 XOR 20). 

## Safe pagemap based detector [experimental]

We are currently testing a new detector that uses the safer pagemap interface instead of using /dev/mem. The new detector can be found in the repository: mc-mapping-pagemap.c.

The following is the result of the new detector on the Nehalem platform we used in the original PALLOC paper, which clearly shows bit 12,13,19,20 are used for the mapping. 

	$ make mc-mappgin-pagemap
	gcc mc-mapping-pagemap.c -Wall -O2 -std=c11 -o mc-mapping-pagemap -lrt -lpthread -g

	$ sudo chrt -f 1 ./mc-mapping-pagemap -p 0.7 -n 3
	mem_size (MB): 2756
	allocation complete.
	worker thread begins
	worker thread begins
	worker thread begins
	Bit6: 299.67 MB/s, 213.57 ns
	Bit7: 307.61 MB/s, 208.05 ns
	Bit8: 295.97 MB/s, 216.24 ns
	Bit9: 297.84 MB/s, 214.88 ns
	Bit10: 300.66 MB/s, 212.86 ns
	Bit11: 245.89 MB/s, 260.28 ns
	Bit12: 792.58 MB/s, 80.75 ns	<--- slower
	Bit13: 789.23 MB/s, 81.09 ns	<--- slower
	Bit14: 296.21 MB/s, 216.06 ns
	Bit15: 294.19 MB/s, 217.55 ns
	Bit16: 240.98 MB/s, 265.58 ns
	Bit17: 294.20 MB/s, 217.54 ns
	Bit18: 294.07 MB/s, 217.64 ns
	Bit19: 789.05 MB/s, 81.11 ns	<--- slower
	Bit20: 789.15 MB/s, 81.10 ns	<--- slower
	Bit21: 294.17 MB/s, 217.56 ns
	Bit22: 240.98 MB/s, 265.58 ns
	Bit23: 294.10 MB/s, 217.62 ns

We recommend using this (mc-mapping-pagemap) over the original mc-mapping. Note, however, that it currently does not support XOR mapping detection. 

## Address map database
A set of successfully identified address map information can be found in the following wiki page. 

https://github.com/heechul/palloc/wiki/Address-map-database

## Limitations and other detection methods
Our reverse engineering method does not work with many modern memory controllers which utilize more sophisticated XOR mapping schemes.

If you are unable to detect the DRAM address mapping with the tools we provide here, consider checking the following alternatives.

DRAMA
https://github.com/IAIK/drama

DRAMDig
https://arxiv.org/pdf/2004.02354

Blacksmith
https://github.com/comsec-group/blacksmith

References
==========

[1] H. Yun, R. Mancuso, Z. Wu, R. Pellizzoni, "PALLOC: DRAM Bank-Aware Memory Allocator for Performance Isolation on Multicore Platforms", _RTAS_, 2014.

[2] Z. Zhang, Z. Zhu, X. Zhang, "A permutation-based page interleaving scheme to reduce row-buffer conflicts and exploit data locality", _MICRO_, 2000
