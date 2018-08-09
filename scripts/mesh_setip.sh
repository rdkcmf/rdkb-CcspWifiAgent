#!/bin/sh
######################################################################################
# If not stated otherwise in this file or this component's Licenses.txt file the
# following copyright and licenses apply:

#  Copyright 2018 RDK Management

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#######################################################################################

MODEL_NUM=`grep MODEL_NUM /etc/device.properties | cut -d "=" -f2`
arch="onecpu"
if [ "$MODEL_NUM" == "DPC3941" ] || [ "$MODEL_NUM" == "TG1682G" ] || [ "$MODEL_NUM" == "DPC3939" ]; then
 arch="puma6";
fi

if [ "$arch" == "puma6" ]; then
 IF_MESHBR24=`wifi_api wifi_getApBridgeInfo 12 "" "" "" | head -n 1`
 IF_MESHBR50=`wifi_api wifi_getApBridgeInfo 13 "" "" "" | head -n 1`
else
 IF_MESHBR24=`wifi_api wifi_getApName 12`
 IF_MESHBR50=`wifi_api wifi_getApName 13`
fi

#XF3 specific changes
if [ "$MODEL_NUM" == "PX5001" ]; then
 IF_MESHBR24="brlan112"
 IF_MESHBR50="brlan113"
 IF_MESHVAP24="wl0.6"
 IF_MESHVAP50="wl1.6"
 PLUME_BH1_NAME="brlan112"
 PLUME_BH2_NAME="brlan113"
 DEFAULT_PLUME_BH1_IPV4_ADDR="169.254.0.1"
 DEFAULT_PLUME_BH2_IPV4_ADDR="169.254.1.1"
 DEFAULT_PLUME_BH_NETMASK="255.255.255.0"
fi
MESHBR24_IP="169.254.0.1 netmask 255.255.255.0"
MESHBR50_IP="169.254.1.1 netmask 255.255.255.0"
BRIDGE_MTU=1600

mesh_bridges()
{

echo "ADD wifi interfaces to the Bridge $PLUME_BH1_NAME" > /dev/console
for WIFI_IFACE in $IF_MESHVAP24
do
   /sbin/ifconfig $WIFI_IFACE down
done

for WIFI_IFACE in $IF_MESHVAP24
do
   brctl addif $PLUME_BH1_NAME $WIFI_IFACE
   /sbin/ifconfig $WIFI_IFACE 0.0.0.0 up
done

echo inf add wl0.6 > /proc/driver/flowmgr/cmd

echo 1 > /sys/class/net/$PLUME_BH1_NAME/bridge/nf_call_iptables
echo 1 > /sys/class/net/$PLUME_BH1_NAME/bridge/nf_call_arptables
   /sbin/ifconfig $PLUME_BH1_NAME $DEFAULT_PLUME_BH1_IPV4_ADDR netmask $DEFAULT_PLUME_BH_NETMASK up


echo "ADD wifi interfaces to the Bridge $PLUME_BH2_NAME" > /dev/console
for WIFI_IFACE in $IF_MESHVAP50
do
   /sbin/ifconfig $WIFI_IFACE down
done

for WIFI_IFACE in $IF_MESHVAP50
do
   brctl addif $PLUME_BH2_NAME $WIFI_IFACE
   /sbin/ifconfig $WIFI_IFACE 0.0.0.0 up
done

echo inf add wl1.6 > /proc/driver/flowmgr/cmd

echo 1 > /sys/class/net/$PLUME_BH2_NAME/bridge/nf_call_iptables
echo 1 > /sys/class/net/$PLUME_BH2_NAME/bridge/nf_call_arptables
   /sbin/ifconfig $PLUME_BH2_NAME $DEFAULT_PLUME_BH2_IPV4_ADDR netmask $DEFAULT_PLUME_BH_NETMASK up

}

is_vlan() {
    ifn="$1"
    [ -z "$ifn" ] && return 1

    ip -d link show $ifn | grep vlan > /dev/null
    return $?
}

vlan_root() {
    echo "$1" | cut -d '.' -f 1
}

bridge_interfaces() {
    br_ifn="$1"
    [ -z "$br_ifn" ] && return

    brctl show $br_ifn | grep -v STP | while read a b c d; do
        [ ${#d} -eq 0 ] && echo $a || echo $d
    done
}

bridge_set_mtu() {
    br_ifn="$1"
    br_mtu="$2"
    [ -z "$br_ifn" -o -z "$br_mtu" ] && return

    echo "...Setting bridge $br_ifn MTU to $br_mtu"
    for ifn in $(bridge_interfaces $br_ifn); do
        if is_vlan $ifn; then
            echo "......Setting root $(vlan_root $ifn) MTU to $br_mtu"
            ifconfig $(vlan_root $ifn) mtu $br_mtu
        fi
        echo "......Setting $(vlan_root $ifn) MTU to $br_mtu"
        ifconfig $ifn mtu $br_mtu
    done

    ifconfig $br_ifn $br_mtu
}

if [ -n "${IF_MESHBR24}" ]; then
    echo "Configuring $IF_MESHBR24"
    if [ "$arch" == "puma6" ]; then
     bridge_set_mtu $IF_MESHBR24 $BRIDGE_MTU
    fi
    ifconfig $IF_MESHBR24 $MESHBR24_IP
    if [ "$MODEL_NUM" == "PX5001" ]; then
     ifconfig $IF_MESHBR24 mtu $BRIDGE_MTU
     ifconfig $IF_MESHVAP24 mtu $BRIDGE_MTU
    fi
fi

if [ -n "${IF_MESHBR50}" ]; then
    echo "Configuring $IF_MESHBR50"
    if [ "$arch" == "puma6" ]; then
     bridge_set_mtu $IF_MESHBR50 $BRIDGE_MTU
    fi
    ifconfig $IF_MESHBR50 $MESHBR50_IP
    if [ "$MODEL_NUM" == "PX5001" ]; then
     ifconfig $IF_MESHBR50 mtu $BRIDGE_MTU
     ifconfig $IF_MESHVAP50 mtu $BRIDGE_MTU
    fi
fi
if [ "$MODEL_NUM" == "PX5001" ]; then
	brctl113=`brctl show | grep wl1.6`
        brctl112=`brctl show | grep wl0.6`
        if [ "$brctl113" == "" ] || [ "$brctl112" == "" ]; then
 		mesh_bridges
        fi
fi
