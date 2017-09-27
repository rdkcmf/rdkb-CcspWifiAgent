# This script is used to config the SSID.13, SSID.14 for mesh backhaul GRE
# by zhicheng_qiu@comcast.com
#!/bin/sh
#RDKB-13835: Sometimes race conditions prevents bringing up of ath12 and ath13
wifi_api wifi_setApEnable 12 1
wifi_api wifi_setApEnable 13 1

#MESH-223
uapsd=`wifi_api wifi_getApWmmUapsdEnable 12 | head -n 1`
if [ "$uapsd" != "FALSE" ]; then
	wifi_api wifi_setApWmmUapsdEnable 12 0
	wifi_api wifi_setApWmmUapsdEnable 13 0
fi

brname=`wifi_api wifi_getApBridgeInfo 12 "" "" "" | head -n 1`
if [ "$brname" == "br12" ]; then
	exit 0;
fi

for idx in 12 13
do

	if [ "$idx" == "12" ]; then
		brname="br12"
		vlan=112
	else
		brname="br13"
		vlan=113
	fi

	#AP_BRNAME_13:=.
	wifi_api wifi_setApBridgeInfo  $idx $brname "" ""

	#AP_VLAN_13:=
	wifi_api wifi_setApVlanID $idx $vlan

	#AP_STATIONISOLATE_13:=0
	#wifi_api wifi_setAPIsolationEnable $idx 0

	#AP_HIDESSID_13:=1
	wifi_api wifi_setApSsidAdvertisementEnable   $idx 0

	#AP_SECMODE_13:=WPA
	#AP_WPA_13:=3
	wifi_api wifi_setApBeaconType $idx "WPAand11i"

	#AP_SECFILE_13:=PSK
	wifi_api wifi_setApBasicAuthenticationMode $idx "PSKAuthentication"

	#AP_CYPHER_13:=TKIP CCMP
	wifi_api wifi_setApWpaEncryptionMode $idx "TKIPandAESEncryption"

	#AP_SSID_13:=we.piranha
	wifi_api wifi_setSSIDName $idx "we.piranha.off"

	#PSK_KEY_13:=welcome8
	wifi_api wifi_setApSecurityPreSharedKey $idx "welcome8"

	#WPS_ENABLE_13:=0
	wifi_api wifi_setApWpsEnable $idx 0

	#AP_ENABLE_13:=1
	wifi_api wifi_setApEnable  $idx 1

done

