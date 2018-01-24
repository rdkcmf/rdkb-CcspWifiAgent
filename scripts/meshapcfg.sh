# This script is used to config the SSID.13, SSID.14 for mesh backhaul GRE
# by zhicheng_qiu@comcast.com
# prash: modified script for writing the mesh para after reading the current value

MODEL_NUM=`grep MODEL_NUM /etc/device.properties | cut -d "=" -f2`
enable_AP="TRUE"
if [ "$MODEL_NUM" == "DPC3941" ] || [ "$MODEL_NUM" == "TG1682G" ]; then
 if [ `grep mesh_enable /nvram/syscfg.db | cut -d "=" -f2` != "true" ]; then
  echo "Mesh Disabled, Dont bringup Mesh interfces"
  enable_AP="FALSE"
 fi
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

        
        uapsd=`wifi_api wifi_getApWmmUapsdEnable $idx | head -n 1`
        if [ "$uapsd" != "FALSE" ]; then
	 wifi_api wifi_setApWmmUapsdEnable $idx 0
	fi

        #AP_BRNAME_13:=.
        if [ `wifi_api wifi_getApBridgeInfo $idx "" "" "" | head -n 1` != "$brname" ]; then
         wifi_api wifi_setApBridgeInfo  $idx $brname "" ""
        fi
        
        wifi_api wifi_setApVlanID $idx $vlan
        
        if [ `wifi_api wifi_getApSsidAdvertisementEnable $idx` != "FALSE" ]; then
         wifi_api wifi_setApSsidAdvertisementEnable $idx 0
        fi

        if [ `wifi_api wifi_getApBeaconType $idx` == "None" ]; then
         wifi_api wifi_setApBeaconType $idx "WPAand11i"
        fi

        #AP_SECFILE_13:=PSK
        wifi_api wifi_setApBasicAuthenticationMode $idx "PSKAuthentication"
      
        if [ `wifi_api wifi_getApWpaEncryptionMode $idx` != "TKIPandAESEncryption" ]; then
         wifi_api wifi_setApWpaEncryptionMode $idx "TKIPandAESEncryption"
        fi

        if [ `wifi_api wifi_getSSIDName $idx` != "we.piranha.off" ]; then
          wifi_api wifi_setSSIDName $idx "we.piranha.off"
        fi
 
        #PSK_KEY_13:=welcome8
        if [ "$MODEL_NUM" == "DPC3941" ] || [ "$MODEL_NUM" == "TG1682G" ]; then
         if [ -z `wifi_api wifi_getApSecurityPreSharedKey $idx` ]; then
          wifi_api wifi_setApSecurityPreSharedKey $idx "welcome8"
         fi
        else
         if [ `wifi_api wifi_setApSecurityPreSharedKey $idx` != "welcome8" ]; then
          wifi_api wifi_setApSecurityPreSharedKey $idx "welcome8"
         fi
        fi

        if [ `wifi_api wifi_getApWpsEnable $idx` != "FALSE" ]; then
         wifi_api wifi_setApWpsEnable $idx 0
        fi
        
        if [ "$enable_AP" == "TRUE" ] ; then
         if [ `wifi_api wifi_getApEnable $idx` != "TRUE" ]; then
          wifi_api wifi_setApEnable $idx 1
         fi
        else
         if [ `wifi_api wifi_getApEnable $idx` != "FALSE" ]; then
          wifi_api wifi_setApEnable $idx 0
         fi
        fi
done

