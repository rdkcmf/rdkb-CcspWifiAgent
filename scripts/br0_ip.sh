#!/bin/sh
#zhicheng_qiu@comcast.com
#this script is for Puma6 platform only

. /etc/device.properties

if [ "$2" != "" ] ; then
	ip=$1;
	mask=$2;
else
	ip=`psmcli get dmsb.atom.l3net.4.V4Addr`
	mask=`psmcli get dmsb.atom.l3net.4.V4SubnetMask`
fi

net=`echo $ip | cut -d"." -f1,2,3`.0
if [ "$mask" == "255.255.255.128" ]; then
	nmask=25;
elif [ "$mask" == "255.255.0.0" ]; then
	nmask=16;
elif [ "$mask" == "255.0.0.0" ]; then
	nmask=8;
else
	nmask=24;
fi

ifconfig br0 $ip netmask $mask up
ip route add table main  $net/$nmask dev br0  proto kernel  scope link  src $ip
ip route add table local local $ip dev br0  proto kernel  scope host  src $ip
ip route del 224.0.0.0/4 dev eth0
ip route add 224.0.0.0/4 dev br0

#DNS
#rpcclient $ARM_ARPING_IP "cat /etc/resolv.conf" | grep nameserver | grep -v 127.0.0.1 > /etc/resolv.conf

