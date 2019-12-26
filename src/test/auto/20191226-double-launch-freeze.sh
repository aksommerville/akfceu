#!/bin/sh

RASERVER="http://localhost:8111"

echo "" > /Users/andy/proj/akfceu/fakelog

echo "First launch (via open)..."
#curl -X POST "$RASERVER/ipc/launch?fileid=143"
open "/Users/andy/rom/nes/z/zelda.nes" --args --reopen-tty=/dev/null #/Users/andy/proj/akfceu/fakelog

echo "Sleeping 2 seconds..."
sleep 2

echo "Second launch (via curl)..."
curl -X POST "$RASERVER/ipc/launch?fileid=143"

echo "Sleeping 2 seconds..."
sleep 2
echo "Dumping log..."
cat /Users/andy/proj/akfceu/fakelog

#echo "Sleeping 5 seconds..."
#sleep 5

#echo "Third launch (via curl)..."
#curl -X POST "$RASERVER/ipc/launch?fileid=143"

