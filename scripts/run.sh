#!/bin/bash
export JAVAB_THREADS=$1
echo "Executing for $1 Threads"
echo "****************Sequential*******************"
java ByteTest
echo "************Fork/Join(Custom)****************"
java  CustomForkJoin $1
echo "************Fork/Join (JAVAB)****************"
./javab.sh 2> /dev/null > /dev/null & 
java -noverify Threading
echo "************Executor Service ****************"
java Executor $1
echo ""
