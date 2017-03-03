# javab-agent
This repository contains the code of a JVMTI agent which automatically analyses the JVM bytecode during runtime and exploit the implicit loop parallelism in the code and parallelize it on the fly. 

###Introduction
The analysis of the bytecode heavily relies on an open source bytecode parallelization tool [JAVAB](http://www.aartbik.com/JAVAB/). It was made to parallelize the JVM bytecode statically. It does a static analysis of the compiled bytecode (by JAVAC), finds implicit loop parallelism in the code and exploits it by doing a hard coded parallelization in the class file by modifying it in the file system. This tool adds the capability of runtime analysis to the JAVAB. It hooks with the JVM during startup and takes out the class file for every loading class. It then analyses for parallelizability, prallelizes the bytecode and inserts the instrumented class file inside JVM directly.

###Why runtime?
Although there is bit of an overhead for runtime analysis. But the parallelization analysis mechanism provided in the [JAVAB](http://www.aartbik.com/JAVAB/) tackles the problem of runtime overhead beautifully. It does a very minimal necessary analysis with an assumption that "Nothing is parallelizable in general". It does a very brief and to the point analysis to determine parallelizability potential in the bytecode. 
On the bright side, runtime analysis provides some extra profiling information of the executing code which can be incorporated with the analysis to make it more subtle and efficient. For example, parallelizing all the loops (parallelizable) actually gives an overhead instead of speeding up the program. If we can manage to use the profiler information for hotspots in the code, we'll be able to analyse less (reducing analysis overhead) and parallelize only the potential code which is consuming longer time on CPU (Hotspot recognition is still in progress).

###Proposed Features
1. Automatically detect implicit loop parallelizabilty in the bytecode. (Done)
2. Automatically exploit the parallel loops by dividing them in multiple threads. (Done)
3. Provide a feedback about non-parallelizable loops and determine the cause of non-parallelizability. (Done)
4. Parallelize user annotated loops without analysis. (Under Progress)
5. Leverage the runtime information of profiler to analyse only the hotspots in the code. (Under Progress)

###Building the Code
First, you'll have to edit the Makefile for for the correct path of jvmti.h and jni.h header files. Edit the variables `JVMTI_PATH` and `JVMTI_PATH_LINUX` to set the path to your respective JDK directory.
Building the code is pretty straight forward afterwards. Simply execute the following command in the main repository directory:
```Bash
make
```
You should have GNU make installed as a prerequisite.
###Using the Agent
```Bash
java -agentpath:/path/to/agent -cp /tmp:/your/class/path test.class
```
###Preliminary Tests
Some tests were performed on various machines with varying processor cores for the matrix multiplication code in java. The results are self explanatory. These tests were performed with threads-time and problem_size-time perspectives.
The code used for these tests is given in the tests folder in the repository.
[![Server Problem_size-time][1]][1]
===========================
Following tests were performed with a fixed matrix size of 1000x1000 for both the matrices.
[![Parallelization on Laptop][2]][2]
[![Server thread-time][3]][3]
[![Summary][4]][4]

The scripts used to take these samples and generate the graphs are given in the scripts folder. You can use these scripts to generate the graphs on your machine for varying problem sizes and number of threads. Graphs were generated using [Scilab](https://www.scilab.org/) scripts by reading the csv files that were sampled using bash scripts.
[1]: https://github.com/saqibahmed515/javab-agent/blob/master/48_threads_multi_dim.png
[2]: https://github.com/saqibahmed515/javab-agent/blob/master/laptop_dev_jvm.png
[3]: https://github.com/saqibahmed515/javab-agent/blob/master/server_dev_jvm.png
[4]: https://github.com/saqibahmed515/javab-agent/blob/master/summary_above_14_threads.png
