#!/bin/bash
for i in {1..30}
do
	strace -c -o log ./part1.x
	cat log | grep 'total'
done
