#! /bin/bash

BINARY="$1"
TEST="$(basename "$BINARY")"
DATA="$2"/data
DETAIL="${3:-0}"

result=0

for testcase in "$DATA"/*.in; do
	expected="${testcase%.*}".out
	diff="$(diff "$expected" <("$BINARY" -t < "$testcase"))"
	if [ $? -ne 0 ]; then
		result=1
		echo "$TEST: fail: $(basename "$testcase")"
		if [ "$DETAIL" -gt 0 ]; then
			echo "$diff"
		fi
	fi
done

exit $result
