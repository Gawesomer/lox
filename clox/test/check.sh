#!/bin/sh
#
# Make sure to build clox without debug logs pior to running this script
#
# Ignore: benchmark, expressions, limit, scanning

#set -x

test_file()
{
   output=$(diff -u <(../main $1 2>&1) <(./output.awk $1))
   if [ $? -eq 1 ]
   then
       echo
       echo $1
       echo -e $output
       return 1
   else
       echo -n "."
       return 0
   fi
}

if [ "$#" -eq 1 ]
then
    test_file $1
else
    numfailed=0
    for f in $(find . -name "*.lox" -not -path "*[benchmark|expressions|limit|scanning]/*")
    do
        test_file $f
        numfailed=$(($numfailed + $?))
    done
    echo
    echo "Number of test failed: " $numfailed
fi
