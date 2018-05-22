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
 IF_MESHBR24=`wifi_api wifi_getApName 0`
 IF_MESHBR50=`wifi_api wifi_getApName 1`
fi

MESHBR24_IP="169.254.0.1 netmask 255.255.255.0"
MESHBR50_IP="169.254.1.1 netmask 255.255.255.0"
BRIDGE_MTU=1600

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
fi

if [ -n "${IF_MESHBR50}" ]; then
    echo "Configuring $IF_MESHBR50"
    if [ "$arch" == "puma6" ]; then
     bridge_set_mtu $IF_MESHBR50 $BRIDGE_MTU
    fi
    ifconfig $IF_MESHBR50 $MESHBR50_IP
fi
