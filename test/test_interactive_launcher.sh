#!/bin/sh

myPID=$$
trap "killall -9 ./test_interactive.bin ; kill -9 $myPID" 2
echo killing old processes
killall -9 ./test_interactive.bin test_interactive.bin 2>&1 > /dev/null

./test_interactive.bin &
lePid=`pidof ./test_interactive.bin`
echo pid is $lePid
echo "waiting for process to initialize itself..."
echo 3
sleep 1 
echo 2
sleep 1 
echo 1
sleep 1 
echo "beginning test"

echo sending to $lePid
kill -INT $lePid
sleep 4
echo sending to $lePid
kill -INT $lePid
sleep 1
echo sending to $lePid
kill -INT $lePid
sleep 1
echo sending to $lePid
kill -INT $lePid
sleep 1

echo sending to $lePid
kill -INT $lePid
sleep 1
echo sending to $lePid
kill -INT $lePid
sleep 3
echo sending to $lePid
kill -INT $lePid
sleep 5
echo sending to $lePid
kill -INT $lePid
sleep 1

sleep 15
echo sending to $lePid
kill -INT $lePid
echo sending to $lePid
kill -INT $lePid
sleep 5
echo terminating $lePid
#kill -9 $lePid
kill -USR1 $lePid
# wait a long while because tasks need to finish
sleep 14
pidof test_interactive.bin &&  echo "process stil exists?" || echo "exited correctly" 
