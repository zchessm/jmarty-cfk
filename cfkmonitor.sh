#!/bin/bash

host="localhost"
port="5555"
time="10"

inst1="./perfToolMod/perfClient $host $port $time"
inst2="./perfToolMod/perfClient $host $(($port + 1)) $time"
inst3="./perfToolMod/perfClient $host $(($port + 2)) $time"

data=$(($inst1 & $inst2 & $inst3) | grep '^[0-9]' | awk '{print $2, $3, $4}')
tokens=($data)

min=$(echo "${tokens[0]} + ${tokens[3]} + ${tokens[6]}" | bc)
max=$(echo "${tokens[1]} + ${tokens[4]} + ${tokens[7]}" | bc)
avg=$(echo "${tokens[2]} + ${tokens[5]} + ${tokens[8]}" | bc)

ip=$(dig TXT +short o-o.myaddr.l.google.com @ns1.google.com | awk -F'"' '{print $2}')
mc=$(ip link show | awk '/ether/ {print $2}' | head -n 1)
dt=$(date "+%F %T")

id=`mysql -ss -N -u khattle -h mysql1.cs.clemson.edu --password=420noscopeblazeit ssadatabase -e "INSERT INTO Iterations ( IP, Time, RTTMin, RTTMax, RTTAverage, ID ) VALUES ( '$ip', '$dt', '$min', '$max', '$avg', '$mc' );"`
