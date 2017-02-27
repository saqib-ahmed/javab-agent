#!/bin/bash
export JAVAB_THREADS=$1
PAR_JAVA="/home/saqib/Dev_active/repo/jdk8u66_sf/build/linux-x86_64-normal-server-slowdebug/jdk/bin"
ARGS="-XX:CompileOnly=Threading.main,Threading\$1.run -XX:TieredStopAtLevel=1"
PAR="-Djava.util.concurrent.ForkJoinPool.common.parallelism=$1"
printf '%d,' $1
$PAR_JAVA/java ByteTest
printf ','
$PAR_JAVA/java  CustomForkJoin $1
printf ',' 
./javab.sh 2> /dev/null > /dev/null & 
$PAR_JAVA/java -noverify Threading
printf ','
$PAR_JAVA/java Executor $1
printf ','
$PAR_JAVA/java $PAR Parallel_stream
#printf ',' 
#$PAR_JAVA/java $ARGS -cp ~/workspace/Threading/bin Threading $1 
printf '\n'
