all:
.SILENT:
PRECMD=echo "  $(@F)" ; mkdir -p $(@D) ;

#------------------------------------------------------------------------------
# Compilers, configuration, etc.
# This Makefile supports only a single target architecture.
# You can configure for different targets by modifying this section.
# Nothing outside this section should need to be changed.
# We try to assume as little as possible: No autoconf, no incoming environment variables, etc.

# Output directories. Must begin with 'mid' and 'out'.
MIDDIR:=mid
OUTDIR:=out

# GCC toolchain.
CCWARN:=-Werror -Wimplicit -Wno-pointer-sign -Wno-parentheses-equality -Wno-parentheses
CCINCLUDE:=-Isrc -I$(MIDDIR)
CCDECL:=-DHAVE_ASPRINTF=1 -DPSS_STYLE=1 -DUSE_macos=1 -DLSB_FIRST=1
CC:=gcc -c -MMD -O2 $(CCINCLUDE) $(CCWARN) $(CCDECL)
OBJC:=gcc -xobjective-c -c -MMD -O2 $(CCINCLUDE) $(CCWARN) $(CCDECL)
LD:=gcc
LDPOST:=-lz -framework AudioUnit -framework IOKit -framework Cocoa -framework OpenGL

# Main and unit-test executables.
#EXE:=$(OUTDIR)/fceu
TEST:=$(OUTDIR)/test

# How to launch the main program.
#RUNCMD:=$(EXE)

# Directories immediately under <src/opt> to include in the build.
OPT:=macos

# Only relevant for MacOS:
BUNDLE_MAIN:=$(OUTDIR)/akfceu.app
EXE:=$(BUNDLE_MAIN)/Contents/MacOS/akfceu
PLIST_MAIN:=$(BUNDLE_MAIN)/Contents/Info.plist
NIB_MAIN:=$(BUNDLE_MAIN)/Contents/Resources/Main.nib
ICON_MAIN:=$(BUNDLE_MAIN)/Contents/Resources/appicon.icns

$(EXE):$(PLIST_MAIN) $(NIB_MAIN)
$(PLIST_MAIN):src/opt/macos/Info.plist;$(PRECMD) cp $< $@
$(NIB_MAIN):src/opt/macos/Main.xib;$(PRECMD) ibtool --compile $@ $<

INPUT_ICONS:=$(wildcard src/opt/macos/appicon.iconset/*)
$(ICON_MAIN):$(INPUT_ICONS);$(PRECMD) iconutil -c icns -o $@ src/opt/macos/appicon.iconset

RUNCMD:=open -W $(BUNDLE_MAIN) --args --reopen-tty=$$(tty) --chdir=$$(pwd)

#------------------------------------------------------------------------------
# Generated source files.
# These must be located under $(MIDDIR), and you must provide a rule to generate them.

GENERATED_FILES:= \
  $(MIDDIR)/test/akfceu_test_contents.h

#------------------------------------------------------------------------------
# Everything below this point should work for any target.

OPTAVAILABLE:=$(notdir $(wildcard src/opt/*))
OPTIGNORE:=$(filter-out $(OPT),$(OPTAVAILABLE))
MIDOPTIGNORE:=$(addprefix $(MIDDIR)/,$(addsuffix /%,$(OPTIGNORE)))
SRCOPTIGNORE:=$(addprefix src/,$(addsuffix /%,$(OPTIGNORE)))

SRCCFILES:=$(filter-out $(SRCOPTIGNORE),$(shell find src -name '*.[cm]'))
OPTCFILES:=$(filter src/opt/%,$(SRCCFILES))
MAINCFILES:=$(filter src/main/%,$(SRCCFILES))
TESTCFILES:=$(filter src/test/%,$(SRCCFILES))
CORECFILES:=$(filter-out $(OPTCFILES) $(MAINCFILES) $(TESTCFILES),$(SRCCFILES))

GENERATED_FILES:=$(filter-out $(MIDOPTIGNORE),$(GENERATED_FILES))
GENHFILES:=$(filter %.h,$(GENERATED_FILES))
GENCFILES:=$(filter %.c,$(GENERATED_FILES))
GENOPTCFILES:=$(filter $(MIDDIR)/opt/%,$(GENCFILES))
GENMAINCFILES:=$(filter $(MIDDIR)/main/%,$(GENCFILES))
GENTESTCFILES:=$(filter $(MIDDIR)/test/%,$(GENCFILES))
GENCORECFILES:=$(filter-out $(GENOPTCFILES) $(GENMAINCFILES) $(GENTESTCFILES),$(GENCFILES))

COREOFILES:=$(patsubst src/%,$(MIDDIR)/%.o,$(basename $(CORECFILES))) $(GENCOREOFILES:.c=.o)
TESTOFILES:=$(patsubst src/%,$(MIDDIR)/%.o,$(basename $(TESTCFILES))) $(GENTESTCFILES:.c=.o)
MAINOFILES:=$(patsubst src/%,$(MIDDIR)/%.o,$(basename $(MAINCFILES))) $(GENMAINCFILES:.c=.o)
OPTOFILES:=$(patsubst src/%,$(MIDDIR)/%.o,$(basename $(OPTCFILES))) $(GENOPTCFILES:.c=.o)
ALLOFILES:=$(COREOFILES) $(TESTOFILES) $(MAINOFILES) $(OPTOFILES)
-include $(ALLOFILES:.o=.d)

$(MIDDIR)/%.o:src/%.c|$(GENHFILES);$(PRECMD) $(CC) -o $@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.c|$(GENHFILES);$(PRECMD) $(CC) -o $@ $<
$(MIDDIR)/%.o:src/%.m|$(GENHFILES);$(PRECMD) $(OBJC) -o $@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.m|$(GENHFILES);$(PRECMD) $(OBJC) -o $@ $<

all:$(EXE) $(TEST)
$(EXE):$(COREOFILES) $(MAINOFILES) $(OPTOFILES);$(PRECMD) $(LD) -o $@ $(COREOFILES) $(MAINOFILES) $(OPTOFILES) $(LDPOST)
$(TEST):$(COREOFILES) $(TESTOFILES) $(OPTOFILES);$(PRECMD) $(LD) -o $@ $(COREOFILES) $(MAINOFILES) $(OPTOFILES) $(LDPOST)

#------------------------------------------------------------------------------

run:$(EXE);$(RUNCMD)
test:$(TEST);$(TEST)

clean:;rm -rf mid out

$(MIDDIR)/test/akfceu_test_contents.h:etc/make/gentest.sh $(TESTCFILES) $(GENTESTCFILES);$(PRECMD) $^ $@
