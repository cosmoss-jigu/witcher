#!/bin/bash
start_idx=$1
skip_idx=$2
file=$3

count=0
while IFS= read -r line
do
	if [ "$count" -lt "$start_idx"  -o   "$count" -eq "$skip_idx" ]
	then
		((count++))
		continue
	fi
	((count++))
	op="$(cut -d';' -f1 <<<"$line")"
	if [ $op == "i" ]
	then
		key="$(cut -d';' -f2 <<<"$line")"
		value="$(cut -d';' -f3 <<<"$line")"
		echo "set ${key} ${value}"
	elif [ $op == "g" ]
	then
		key="$(cut -d';' -f2 <<<"$line")"
		echo "get ${key}"
	elif [ $op == "d" ]
	then
		key="$(cut -d';' -f2 <<<"$line")"
		echo "del ${key}"
	fi
done <"$file"
echo "shutdown save"
sleep 3
