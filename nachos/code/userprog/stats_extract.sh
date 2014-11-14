#!/bin/bash
for (( i = 0; i < 4; i++ )); do
#statements
./nachos -rs 0 -A 3 -r $i -x ../test/vmtest1 | grep -e "Ticks:" -e "Paging:"
done