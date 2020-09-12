#!/bin/sh

SUDO=sudo
DSTROOT=/usr/local

if [ -z "$EXE" ] ; then
  echo "Please define EXE"
  exit 1
fi
if [ -z "$UNAMES" ] ; then
  UNAMES="$(uname -s)"
fi

if [ "$UNAMES" = "Darwin" ] ; then
  echo "Looks like we're a Mac. No need to install anything."
  exit 1
fi

EXEDST="$DSTROOT/bin/$(basename $EXE)"
DESKTOPDST="/usr/share/applications/akfceu.desktop"

$SUDO cp "$EXE" "$EXEDST" || exit 1
$SUDO cp etc/linux/akfceu.desktop "$DESKTOPDST" || exit 1
xdg-mime default akfceu.desktop application/x-nes-rom

echo "Installed at $EXEDST. Try double-clicking an NES ROM file."
