#!/bin/sh
ct=10
ip=`ifconfig eth0.500 | grep "inet addr" | cut -d":" -f2 | cut -d"." -f1,2,3`.1;
utc=`date +%s`

l2sping=`ping -c $ct $ip | grep "min/avg/max" | cut -d"=" -f2 | tr -d ' '`

echo "$utc L2S_PING:$l2sping"

