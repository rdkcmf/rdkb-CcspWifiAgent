#!/bin/sh
#This script is used to log parameters for each AP
tm=`date "+%s"`
ifconfig | grep ath | cut -d' ' -f1 | while read ap; do
    sta=`wlanconfig $ap list sta | grep -v ADDR | tr -s " " | awk '{$6=$6-95; print}'`
    if [ "$sta" != "" ]; then
        echo "STA $ap $tm {"
        echo $sta;
        echo "}"
    fi
done

