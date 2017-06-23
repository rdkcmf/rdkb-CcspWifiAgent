#!/bin/sh
#This script is used to log parameters for each AP
ssid1="0"
ssid2="1"
source /etc/log_timestamp.sh

enable1=`wifi_api wifi_getApWpsEnable $ssid1 | grep TRUE`
enable2=`wifi_api wifi_getApWpsEnable $ssid2 | grep TRUE`
configpbc1=`wifi_api wifi_getApWpsConfigMethodsEnabled $ssid1 | grep PushButton`
configpbc2=`wifi_api wifi_getApWpsConfigMethodsEnabled $ssid2 | grep PushButton`
configpin1=`wifi_api wifi_getApWpsConfigMethodsEnabled $ssid1 | grep Keypad`
configpin2=`wifi_api wifi_getApWpsConfigMethodsEnabled $ssid2 | grep Keypad`

if [ "$enable1" != "" ] ; then
	echo_t "RDKB_WPS_ENABLED_1:TRUE"
else
	echo_t "RDKB_WPS_ENABLED_1:FALSE"
fi
if [ "$enable2" != "" ] ; then
	echo_t "RDKB_WPS_ENABLED_2:TRUE"
else
	echo_t "RDKB_WPS_ENABLED_2:FALSE"
fi
if [ "$configpbc1" != "" ] ; then
	echo_t "RDKB_WPS_PBC_CONFIGURED_1:TRUE"
else
	echo_t "RDKB_WPS_PBC_CONFIGURED_1:FALSE"
fi
if [ "$configpbc2" != "" ] ; then
	echo_t "RDKB_WPS_PBC_CONFIGURED_2:TRUE"
else
	echo_t "RDKB_WPS_PBC_CONFIGURED_2:FALSE"
fi
if [ "$configpin1" != "" ] ; then
	echo_t "RDKB_WPS_PIN_CONFIGURED_1:TRUE"
else
	echo_t "RDKB_WPS_PIN_CONFIGURED_1:FALSE"
fi
if [ "$configpin2" != "" ] ; then
	echo_t "RDKB_WPS_PIN_CONFIGURED_1:TRUE"
else
	echo_t "RDKB_WPS_PIN_CONFIGURED_2:FALSE"
fi	
