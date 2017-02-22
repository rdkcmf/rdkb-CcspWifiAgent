#!/bin/sh

IF_MESHBR24=`wifi_api wifi_getApBridgeInfo 12 "" "" "" | head -n 1`
IF_MESHBR50=`wifi_api wifi_getApBridgeInfo 13 "" "" "" | head -n 1`
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
    bridge_set_mtu $IF_MESHBR24 $BRIDGE_MTU
    ifconfig $IF_MESHBR24 $MESHBR24_IP
fi

if [ -n "${IF_MESHBR50}" ]; then
    echo "Configuring $IF_MESHBR50"
    bridge_set_mtu $IF_MESHBR50 $BRIDGE_MTU
    ifconfig $IF_MESHBR50 $MESHBR50_IP
fi
