#!/bin/sh

DEVICE_MODEL=`grep DEVICE_MODEL /etc/device.properties | cut -d"=" -f2`
if [ "$DEVICE_MODEL" != "TCHXB3" ]; then
	exit 0;
fi

port2_9=` cfg -e | grep AP_AUTH_PORT2_9 | cut -d"=" -f2`
port9=` cfg -e | grep AP_AUTH_PORT_9 | cut -d"=" -f2`
port10=` cfg -e | grep AP_AUTH_PORT_10 | cut -d"=" -f2`
port11=` cfg -e | grep AP_AUTH_PORT_11 | cut -d"=" -f2`
port12=` cfg -e | grep AP_AUTH_PORT_12 | cut -d"=" -f2`

if [[ "$port2_9" ==  "0" || "$port2_9" == "" ||
      "$port9" ==  "0" || "$port9" == "" || 
      "$port10" ==  "0" || "$port10" == "" ||
      "$port11" ==  "0" || "$port11" == "" ||
      "$port12" ==  "0" || "$port12" == "" ]]; then
      cfg -a AP_AUTH_PORT_9=1812
      cfg -a AP_AUTH_PORT_10=1812
      cfg -a AP_AUTH_PORT_11=1812
      cfg -a AP_AUTH_PORT_12=1812
      cfg -a AP_AUTH_PORT2_9=1812
      cfg -a AP_AUTH_PORT2_10=1812
      cfg -a AP_AUTH_PORT2_11=1812
      cfg -a AP_AUTH_PORT2_12=1812
      cfg -c
fi
