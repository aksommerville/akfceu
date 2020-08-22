#!/bin/bash

VERBOSITY=2

FAILC=0
PASSC=0
SKIPC=0

log() {
  if [ "$1" -le "$VERBOSITY" ] ; then
    echo -e "$2"
  fi
}

runtest() {
  while IFS= read INPUT ; do
    if [ "${INPUT:0:12}" = "AKFCEU_TEST " ] ; then
      read INTRODUCER COMMAND LOOSE <<<"$INPUT"
      case "$COMMAND" in
        PASS)
          PASSC=$((PASSC+1))
          log 5 "\x1b[32mPASS\x1b[0m $LOOSE"
        ;;
        FAIL)
          FAILC=$((FAILC+1))
          log 1 "\x1b[31mFAIL\x1b[0m $LOOSE"
        ;;
        SKIP)
          SKIPC=$((SKIPC+1))
          log 3 "\x1b[30mSKIP\x1b[0m $LOOSE"
        ;;
        DETAIL)
          log 2 "$LOOSE"
        ;;
        *)
          log 4 "$INPUT"
        ;;
      esac
    else
      log 4 "$INPUT"
    fi
  done < <( $1 $AKFCEU_TEST_FILTER 2>&1 || echo "AKFCEU_TEST FAIL $1 abend" )
}

for f in $* ; do
  runtest $f
done

if [ "$FAILC" -gt 0 ] ; then
  FLAG="\x1b[41m    \x1b[0m"
elif [ "$PASSC" -gt 0 ] ; then
  FLAG="\x1b[42m    \x1b[0m"
else
  FLAG="\x1b[40m    \x1b[0m"
fi
echo -e "$FLAG $FAILC fail, $PASSC pass, $SKIPC skip"
