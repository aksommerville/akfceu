#!/bin/sh

if [ "$#" -lt 1 ] ; then
  echo "Usage: $0 [CFILES...] OUTPUT"
  exit 1
fi

TMPPATH=mid/tmp-itest
rm -f $TMPPATH

while [ "$#" -gt 1 ] ; do
  SRCPATH="$1"
  shift 1
  SRCBASE="$(basename $SRCPATH)"
  nl -ba -s' ' "$SRCPATH" | \
    sed -En 's/^ *([0-9]+) *(XXX_)?AKFCEU_TEST *\( *([a-zA-Z0-9_]*) *(, *([^)]*)\))?.*$/'"$SRCBASE"' \1 _\2 \3 \5/p' >> $TMPPATH
done

DSTPATH="$1"
rm -f $DSTPATH
cat - > $DSTPATH <<EOF
#ifndef AKFCEU_ITEST_CONTENTS_H
#define AKFCEU_ITEST_CONTENTS_H
struct akfceu_itest {
  int enable;
  int (*fn)();
  const char *name;
  const char *file;
  int line;
  const char *groups;
};

EOF

cut -d' ' -f4 $TMPPATH | while read TNAME ; do
  echo "int $TNAME();" >> $DSTPATH
done
echo >> $DSTPATH

echo "static const struct akfceu_itest akfceu_itestv[]={" >> $DSTPATH
while read FNAME LNO IGNORE TNAME GRPS ; do
  if [ "$IGNORE" = _XXX_ ] ; then
    ENABLE=0
  else
    ENABLE=1
  fi
  echo "  {$ENABLE,$TNAME,\"$TNAME\",\"$FNAME\",$LNO,\"$GRPS\"}," >> $DSTPATH
done < $TMPPATH

cat - >> $DSTPATH <<EOF
};

#endif
EOF

rm $TMPPATH
