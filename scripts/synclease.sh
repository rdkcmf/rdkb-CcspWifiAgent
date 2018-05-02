#Requested by meshagent to copy dnsmas.lease from ARM to ATOM 
#for the first time when mesh is enabled, also during plume agent
#restart
ARM_IP=`grep "ARM_INTERFACE_IP" /etc/device.properties | cut -d "=" -f2`
configparamgen jx /etc/dropbear/elxrretyt.swr /tmp/login.swr
scp -i /tmp/login.swr root@$ARM_IP:/nvram/dnsmasq.leases /tmp/dnsmasq.leases
