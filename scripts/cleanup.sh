#!/bin/bash


# Change this to your netid
netid=khg140030

#
# Root directory of your project
PROJDIR=$HOME/Workspace/advos_ass1

#
# This assumes your config file is named "config.txt"
# and is located in your project directory
#
CONFIG=$PROJDIR/scripts/config.txt

#
# Directory your java classes are in
#
BINDIR=$PROJDIR/server

#
# Your main project class
#
PROG=server

n=1

cat $CONFIG | sed -e "s/#.*//" | sed -e "/^\s*$/d" |
(
    read i
    echo $i
    while read line 
    do
        host=$( echo $line | awk '{ print $1 }' )

        echo $host
        ssh $netid@$host killall -u $netid &
        sleep 1

        n=$(( n + 1 ))
    done
   
)


echo "Cleanup complete"
