#!/bin/bash


# Change this to your netid
netid=axm155131

#
# Root directory of your project
PROJDIR=$HOME/CS6378/Project1

CONFIG=$PROJDIR/config.txt

#
# Directory your java classes are in
#
BINDIR=$PROJDIR/bin

#
# Your main project class
#
PROG=myprog.out


cat $CONFIG | sed -e "s/#.*//" | sed -e "/^\s*$/d" | grep "\s*[0-9]\+\s*\w\+.*" |
(
    while read line 
    do
	
	n=$( echo $line | awk '{ print $1 }' )
	host=$( echo $line | awk '{ print $2 }' )
		
	
        ssh -o StrictHostKeyChecking=no $netid@$host $BINDIR/$PROG $n &
    done
)