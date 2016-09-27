#!/bin/sh
#This script is used to log wifi health parameters
interval=900 #15min

run=1
while [ "$run" -eq 1 ]; do
    aphealth.sh >> /rdklogs/logs/wifihealth.txt
    sleep $interval
done

