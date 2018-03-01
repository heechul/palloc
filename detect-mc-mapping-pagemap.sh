#!/bin/bash
killall -9 mc-mapping-pagemap
echo "Run a background task on core1-3"

for c in 1 2; do
	./mc-mapping-pagemap -c $c -w 16 -p 0.2 -i 100000000000 -b 0 >& /dev/null &
done
sleep 1

echo "Now run the test"
for b in `seq 12 23`; do 
	echo -n "Bit$b: "
	./mc-mapping-pagemap -c 0 -p 0.2 -b $b | grep band | awk '{ print $2 }' || echo "N/A"
done
killall -9 mc-mapping-pagemap
