#!/bin/sh
ct=10
ip="192.168.101.1"
utc=`date +%s`

l2sping=`ping -c $ct $ip | grep "min/avg/max" | cut -d"=" -f2 | tr -d ' '`

echo "$utc L2S_PING:$l2sping"

