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

## Final Results:
Following tests were performed finally to see some real world benchmarks in actions. All of these tests were performed on the said server.

| Benchmark/Application   | Tests/ Programs    | 
| :-------------: |:-------------:| 
| Dacapo      | Avrora, batik, jython, luindex, lusearch, sunflow | 
| Scimark      | FFT, SOR, montecarlo, SparseMat Mult, LU      |  
| Specjvm08  | Compress, crypto_aes, crypto_rsa, derby, mpegaudio, sunflow      |   
| Processing  |  AccelrationwithVectors, AnimatedSprite, BlurFilter, Brownian, DepthSort, EdgeDetection, Flocking, Follow3, GravitationalAttraction3D, HashMapClass, MultipleParticle System, Reach3, Reflection, Sequentital-animation, tickle      |  
| Hadoop based examples  |  Fast wavelet transform, wavelet packet transform, word median, word mean, word count      |  
| Java Grande  |  Crypt, Euler, FFT, HeapSort, LU Fact, MolDyn, Monte Carlo, Ray Tracer, Search, Series, SOR, Sparse Mat Mult      |  
| DSP  |  wavFilterFisher, TestPolynomialRootFinder, TestDft, TestIirFilterDesignRandom, irfilterTransferBandpas, IirFilterResponsePlotFisherLowpas      |  

### 2D FFT
The example code we have used, randomly initializes matrix of given size to random integer values and then performs 2D FFT on it in nested loops. 2D FFT calculation with our agent shows significant improvement in performance as the matrix size grows. The loops that were parallelized were nested and computation of real and imaginary parts was done in it. Due to the parallelization of computational work, we see improvement factor also increases from 4-30 times as the image size increase.

![2D FFT](https://github.com/saqibahmed515/javab-agent/blob/master/results/2D_FFT.png)

### Crimmins Speckle Removal
It is implemented in 32 nested loops and each iterates over the whole image, checks and modifies the image according to values. The analysis fails in “array reference chasing”, which verifies that the accessed indices of an array are within bounds, across the iterations of the loop. The minor change in source code was to start the index variable of loop with 1 making lowest array index within bounds. Each condition gave us a target nested loop to parallelize, which created threads equal to number of processors. The “Xprof” output of the program shows that the target function takes most of the time in interpreted as well as in compiled mode. Following graph shows some mixed behavior in performance for Crimmins. We can observe that above 3000 matrix size, parallelization based speedup varies from 1-5 times.

![Crimmins Speckle Removal](https://github.com/saqibahmed515/javab-agent/blob/master/results/crimmins.png)

### Hough Transform
The Hough transform is a technique, which can be used to isolate features of a particular shape within an image such as lines, circles, ellipses, etc. The original source had multiple loops but the main computational loop failed due to data dependence. The target nested loop initializes the matrix by calling `Color.black.getRGB()` and assigning a single constant number. The reason for failing was function call which was replaced by a single constant number, thus making those loops parallelizable. This is a simple example of source, which has initialization rather than any computation in the loop. Following figure shows the unsteady results of Hough test. In this case, parallel execution time remains slightly less or equal to serial.

![Hough](https://github.com/saqibahmed515/javab-agent/blob/master/results/Hough.png)

### Gaussian Smoothing
The Gaussian smoothing is used to blur images and remove detail and noise by convolving the image with Gaussian kernel. The main computation loop occurs when convolution of Gaussian filter kernel is done with the chunks of image. The failing condition was data dependence across the iteration due to read and write over same variable. This convolution mechanism was changed in source code. The target loops, which are parallelized, contain the convolution of the kernel filter and a part of image. The “Xprof” output the program also show the function that performs convolution became hotspot and got compiled. Following figure shows no improvement in the parallel version of Gaussian. As the kernel size tends to be smaller in many practical implementations, the amount of computation that is parallelized is small, resulting in no prominent improvement.

![Gaussian](https://github.com/saqibahmed515/javab-agent/blob/master/results/Gaussian.png)

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
