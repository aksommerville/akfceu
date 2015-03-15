#!/bin/sh

if [ "$#" -lt 1 ] ; then
  echo "Usage: $0 [CFILES...] OUTPUT"
  exit 1
fi

TMPPATH=tmp-unit-tests
rm -f $TMPPATH

while [ "$#" -gt 1 ] ; do
  SRCPATH="$1"
  shift 1
  sed -n 's/^AKFCEU_UNIT_TEST *( *\([a-zA-Z0-9_]*\).*$/\1/p' "$SRCPATH" >> $TMPPATH
done

DSTPATH="$1"
rm -f $DSTPATH
cat - > $DSTPATH <<EOF
#ifndef AKFCEU_TEST_CONTENTS_H
#define AKFCEU_TEST_CONTENTS_H
struct akfceu_test {
  int (*fn)();
  const char *name;
};
EOF

sed 's/^.*$/int &();/' $TMPPATH >> $DSTPATH

echo "static const struct akfceu_test akfceu_testv[]={" >> $DSTPATH
sed 's/^.*$/{&,"&"},/' $TMPPATH >> $DSTPATH

cat - >> $DSTPATH <<EOF
};
#endif
EOF

rm $TMPPATH
