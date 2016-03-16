#!/bin/sh
echo "adding vlan to bridge br106"
brctl addif br106 eth0.106
