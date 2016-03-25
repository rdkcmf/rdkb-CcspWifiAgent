#!/bin/sh
#This script is used to set the L&F pass phrase into the wifi config


configparamgen 5 /tmp/tmp5
configparammod /tmp/tmp5

lpf=`cat /tmp/tmp5.mod`;

rm -f /tmp/tmp5
rm -f /tmp/tmp5.mod

/usr/ccsp/wifi/setPskKey 7 $lpf
/usr/ccsp/wifi/setPskKey 8 $lpf
