#!/bin/bash
echo "********** 4 Threads ************"
echo "Custom Fork Join: "
java CustomForkJoin 4
echo "Executor Service: "
java Executor 4
echo ""
echo "********** 8 Threads ************"
echo "Custom Fork Join: "
java CustomForkJoin 8
echo "Executor Service: "
java Executor 8
echo ""
echo "********** 12 Threads ************"
echo "Custom Fork Join: "
java CustomForkJoin 12
echo "Executor Service: "
java Executor 12
echo ""
echo "********** 16 Threads ************"
echo "Custom Fork Join: "
java CustomForkJoin 16
echo "Executor Service: "
java Executor 16
echo ""
echo "********** 32 Threads ************"
echo "Custom Fork Join: "
java CustomForkJoin 32
echo "Executor Service: "
java Executor 32
echo ""
echo "********** 64 Threads ************"
echo "Custom Fork Join: "
java CustomForkJoin 64
echo "Executor Service: "
java Executor 64
echo ""

