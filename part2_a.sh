#!/bin/bash
cd code/build.linux
echo "Rebuild NachOS"
make clean
make 

cd ../test
make clean
make
../build.linux/nachos -f
../build.linux/nachos -cp FS_test1 /FS_test1
../build.linux/nachos -e /FS_test1
../build.linux/nachos -p /file1
../build.linux/nachos -cp FS_test2 /FS_test2
../build.linux/nachos -e /FS_test2
