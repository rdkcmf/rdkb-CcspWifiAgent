#!/bin/sh
# set 1812 as AP_AUTH_PORT default

port=` cfg -e | grep AP_AUTH_PORT__9 | cut -d"=" -f2`
if [ "$port" ==  "0" ]; then
      cfg -a AP_AUTH_PORT_7=1812
      cfg -a AP_AUTH_PORT_8=1812
      cfg -a AP_AUTH_PORT_9=1812
      cfg -a AP_AUTH_PORT_10=1812
      cfg -a AP_AUTH_PORT_SECONDARY_7=1812
      cfg -a AP_AUTH_PORT_SECONDARY_8=1812
      cfg -a AP_AUTH_PORT_SECONDARY_9=1812
      cfg -a AP_AUTH_PORT_SECONDARY_10=1812
      cfg -c
fi
