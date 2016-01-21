#!/bin/sh

# zqiu >>
# Short term solution for RDKB-3185. 
# Please do not merge this change to stable
#ifconfig br0 10.0.0.2 netmask 255.255.255.0
#ip route add default via 10.0.0.1 dev br0
dhclient br0
ip route del 224.0.0.0/4 dev eth0
ip route add 224.0.0.0/4 dev br0
#zqiu <<

