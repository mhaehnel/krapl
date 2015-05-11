#!/bin/env sh

BC=$(which bc 2>/dev/null)
DOMAIN=0
#This value goes from 1 - 100. Higher value means higher precision but longer measurement time!
PRECISION=100

if [ $? == 1 ]; then
	echo "This program requires bc (bash calculator)"
	exit -2
fi

if  [ $# -lt 1 ]; then
	echo "Please provide domain (PP0 | PP1 | PKG | DRAM) as parameter (second optional parameter is package number)"
	exit -1
fi

if [ $# -eq 2 ]; then
	if [ ! -d /sys/devices/system/rapl/rapl$2 ]; then
		echo "Package $2 not found!"
		exit -3
	fi
	DOMAIN=$2
fi

UNIT=$(< /sys/devices/system/rapl/rapl$DOMAIN/energy_unit)
VAL1=$(< /sys/devices/system/rapl/rapl$DOMAIN/$1/energy)
sleep $(echo $PRECISION*0.01 | $BC -l)
VAL2=$(< /sys/devices/system/rapl/rapl$DOMAIN/$1/energy)
echo "($VAL2 - $VAL1)/(2^$UNIT) * 100/$PRECISION" | $BC -l
