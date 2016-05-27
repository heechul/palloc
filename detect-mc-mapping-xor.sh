#!/bin/bash

killall -9 mc-mapping
echo "Run a background task on core1-3"
./mc-mapping -c 1 -i 100000000000 -b 0 -x >& /dev/null &
./mc-mapping -c 2 -i 100000000000 -b 0 -x >& /dev/null &
./mc-mapping -c 3 -i 100000000000 -b 0 -x >& /dev/null &
sleep 1

echo "Now run the test"
while read buf; do
    lbit=`echo $buf | awk '{ print $1 }'`
    rbit=`echo $buf | awk '{ print $2 }'`
    echo -n "Bit $lbit <--> $rbit: "
    ./mc-mapping -c 0 -i 9000000 -b $lbit -s $rbit -x 2> /dev/null | grep band | awk '{ print $2 }' || echo "N/A"
done
killall -9 mc-mapping
