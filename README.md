# javab-agent
This repository contains the code of a JVMTI agent which automatically analyses the JVM bytecode during runtime and exploit the implicit loop parallelism in the code and parallelize it on the fly. 

## Introduction
The analysis of the bytecode heavily relies on an open source bytecode parallelization tool [JAVAB](http://www.aartbik.com/JAVAB/). It was designed to parallelize JVM Bytecode statically. It does a static analysis of the compiled bytecode (by JAVAC), finds implicit loop parallelism in the code and exploits it by doing a hard coded parallelization in the class file by modifying it in the file system. Javab-agent adds the capability of runtime analysis to the JAVAB. It hooks with the JVM during startup and takes out the class file for every loading class. It then analyses for parallelizability, prallelizes the bytecode and inserts the instrumented class file inside JVM directly. Or alternatively, it can hook up only the hotspots in the code, thus minimizing the analysis overhead.

## Why runtime?
Although there is bit of an overhead for runtime analysis, but the parallelization analysis mechanism provided in the [JAVAB](http://www.aartbik.com/JAVAB/) tackles the problem of runtime overhead beautifully. It does a very minimal necessary analysis with an assumption that "Nothing is parallelizable in general". It does a very brief and to the point analysis to determine parallelizability potential in the bytecode. 
On the bright side, runtime analysis provides some extra profiling information of the executing code which can be incorporated with the analysis to make it more subtle and efficient. For example, parallelizing all the loops (which are parallelizable) actually gives an overhead instead of speeding up the code. If we can manage to use the profiler information for hotspots in the code, we'll be able to analyse less (reducing analysis overhead) and parallelize only the potential code which is consuming longer time on CPU.

## Features
1. Automatically detect implicit loop parallelizabilty in the bytecode.
2. Automatically exploit the parallel loops by dividing the code in the loops in multiple threads. 
3. Provide a feedback about non-parallelizable loops and determine the cause of non-parallelizability.
4. ~Parallelize user annotated loops without analysis.~
5. Leverage the runtime information of profiler to analyse only the hotspots in the code.

## Building the Code
First, you'll have to edit the [Makefile](https://github.com/saqibahmed515/javab-agent/blob/master/Makefile) for for the correct path of jvmti.h and jni.h header files. Edit the variables `JVMTI_PATH` and `JVMTI_PATH_LINUX` to set the path to your respective JDK directory.
Building the code is pretty straight forward afterwards. Simply execute the following command in the main repository directory:
```Bash
make
```
You should have GNU make installed as a prerequisite.

## Using the Agent
```Bash
java -agentpath:/path/to/agent -cp /tmp:/your/class/path test.class
```
## Preliminary Tests

Some tests were performed on various machines with varying processor cores for the matrix multiplication code in java. The results are self explanatory. These tests were performed with threads-time and problem_size-time perspectives.
The code used for these tests is given in the tests folder in the repository.

![Server Problem_size-time](https://github.com/saqibahmed515/javab-agent/blob/master/48_threads_multi_dim.png)

____

Following tests were performed with a fixed matrix size of 1000x1000 for both the matrices.

![Parallelization on Laptop](https://github.com/saqibahmed515/javab-agent/blob/master/laptop_dev_jvm.png)

![Server thread-time](https://github.com/saqibahmed515/javab-agent/blob/master/server_dev_jvm.png)

![Summary](https://github.com/saqibahmed515/javab-agent/blob/master/summary_above_14_threads.png)

The scripts used to take these samples and generate the graphs are given in the scripts folder. You can use these scripts to generate the graphs on your machine for varying problem sizes and number of threads. Graphs were generated using [Scilab](https://www.scilab.org/) scripts by reading the csv files that were sampled using bash scripts.

## System Specs:

### Server:
```
Architecture:      	x86_64
CPU op-mode(s):    	32-bit, 64-bit
Byte Order:        	Little Endian
CPU(s):            	48
On-line CPU(s) list:   	0-47
Thread(s) per core:	2
Core(s) per socket:	12
Socket(s):         	2
NUMA node(s):      	2
Vendor ID:         	GenuineIntel
CPU family:        	6
Model:             	63
Model name:        	Intel(R) Xeon(R) CPU E7-4830 v3 @ 2.10GHz
Stepping:          	4
CPU MHz:           	1200.091
CPU max MHz:       	2700.0000
CPU min MHz:       	1200.0000
BogoMIPS:          	4189.90
Virtualization:    	VT-x
L1d cache:         	32K
L1i cache:         	32K
L2 cache:          	256K
L3 cache:          	30720K
NUMA node0 CPU(s): 	0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46
NUMA node1 CPU(s): 	1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31,33,35,37,39,41,43,45,47
```
### Laptop
```
Architecture:      	x86_64
CPU op-mode(s):    	32-bit, 64-bit
Byte Order:        	Little Endian
CPU(s):            	8
On-line CPU(s) list:   	0-7
Thread(s) per core:	2
Core(s) per socket:	4
Socket(s):         	1
NUMA node(s):      	1
Vendor ID:         	GenuineIntel
CPU family:        	6
Model:             	58
Model name:        	Intel(R) Core(TM) i7-3632QM CPU @ 2.20GHz
Stepping:          	9
CPU MHz:           	1203.259
CPU max MHz:       	3200.0000
CPU min MHz:       	1200.0000
BogoMIPS:          	4390.03
Virtualization:    	VT-x
L1d cache:         	32K
L1i cache:         	32K
L2 cache:          	256K
L3 cache:          	6144K
NUMA node0 CPU(s): 	0-7
```
