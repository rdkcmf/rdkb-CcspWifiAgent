#!/bin/sh
#This script is used to log parameters for each AP
tm=`date "+%s"`
echo "AP $tm {"
ifconfig | grep ath | cut -d' ' -f1 | while read ap; do
    rx=`ifconfig $ap | grep "RX bytes" | cut -d':' -f2 | cut -d' ' -f1`
    if [ "$rx" != 0 ]; then
        echo "$ap RX:$rx"
    fi
done
echo "}"

