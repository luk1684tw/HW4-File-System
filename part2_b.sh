#!/bin/bash
cd code/build.linux
echo "Rebuild NachOS"
make clean
make 

cd ../test
make clean
make

../build.linux/nachos -f
../build.linux/nachos -cp num_100.txt /100
../build.linux/nachos -cp num_1000.txt /1000	
../build.linux/nachos -p /1000
echo "========================================="
../build.linux/nachos -p /100
echo "========================================="
../build.linux/nachos -l /