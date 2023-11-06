all:
.SILENT:
PRECMD=echo "  $(@F)" ; mkdir -p $(@D) ;

#------------------------------------------------------------------------------
# Compilers, configuration, etc.
# This Makefile supports only a single target architecture.
# You can configure for different targets by modifying this section.
# Nothing outside this section should need to be changed.
# We try to assume as little as possible: No autoconf, no incoming environment variables, etc.

UNAMES:=$(shell uname -s)

# Output directories. Must begin with 'mid' and 'out'.
MIDDIR:=mid
OUTDIR:=out

ifeq ($(UNAMES),Darwin)
  CCWARN:=-Werror -Wimplicit -Wno-pointer-sign -Wno-parentheses-equality -Wno-parentheses -Wno-deprecated-declarations
  CCINCLUDE:=-Isrc -I$(MIDDIR) -I../romassist/src
  CCDECL:=-DPSS_STYLE=1 -DLSB_FIRST=1
  CC:=gcc -c -MMD -O2 $(CCINCLUDE) $(CCWARN) $(CCDECL)
  OBJC:=gcc -xobjective-c -c -MMD -O2 $(CCINCLUDE) $(CCWARN) $(CCDECL)
  LD:=gcc
  LDPOST:=../romassist/out/libromassist.a -lz -framework AudioUnit -framework IOKit -framework Cocoa -framework OpenGL

  OPT:=macos romassist
  
  # Macs are kind of a pain in the ass... This bit is Mac-only
  BUNDLE_MAIN:=$(OUTDIR)/akfceu.app
  EXE:=$(BUNDLE_MAIN)/Contents/MacOS/akfceu
  PLIST_MAIN:=$(BUNDLE_MAIN)/Contents/Info.plist
  NIB_MAIN:=$(BUNDLE_MAIN)/Contents/Resources/Main.nib
  ICON_MAIN:=$(BUNDLE_MAIN)/Contents/Resources/appicon.icns

  TEST:=$(OUTDIR)/test
  $(EXE):$(PLIST_MAIN) $(NIB_MAIN)
  $(PLIST_MAIN):etc/macos/Info.plist;$(PRECMD) cp $< $@
  $(NIB_MAIN):etc/macos/Main.xib;$(PRECMD) ibtool --compile $@ $<

  INPUT_ICONS:=$(wildcard etc/macos/appicon.iconset/*)
  $(ICON_MAIN):$(INPUT_ICONS);$(PRECMD) iconutil -c icns -o $@ etc/macos/appicon.iconset

  RUNCMD:=open -W $(BUNDLE_MAIN) --args --reopen-tty=$$(tty) --chdir=$$(pwd) ~/rom/nes/z/zelda.nes

else ifeq ($(UNAMES),Linux)
  CCWARN:=-Werror -Wimplicit -Wno-pointer-sign -Wno-parentheses-equality -Wno-parentheses -Wno-deprecated-declarations -Wno-overflow -Wno-unused-result
  CCINCLUDE:=-Isrc -I$(MIDDIR) -I../ra3/out/include -I/usr/include/libdrm
  CCDECL:=-DPSS_STYLE=1 -DLSB_FIRST=1
  CC:=gcc -c -MMD -O2 $(CCINCLUDE) $(CCWARN) $(CCDECL)
  OBJC:=
  LD:=gcc
  LDPOST:=../ra3/out/libemuhost.a -lz -lX11 -lGLX -lGL -lGLESv2 -lasound -lpthread -lm -lpulse -lpulse-simple -ldrm -lgbm -lEGL
  
  OPT:=linux romassist

  EXE:=$(OUTDIR)/akfceu
  TEST:=$(OUTDIR)/test
  
  RUNCMD:=trap '' INT ; $(EXE) /home/andy/rom/nes/c/castlevania_2.nes #/home/andy/rom/nes/z/zelda.nes
  
  play-%:$(EXE); \
    ROMPATH="$$(find ~/rom/nes -type f -name '*$**' | sed -n 1p)" ; \
    if [ -n "$$ROMPATH" ] ; then \
      $(EXE) "$$ROMPATH" ; \
    else \
      echo "'$*' not found" ; \
    fi
  
else
  CCWARN:=-Werror -Wimplicit -Wno-pointer-sign -Wno-parentheses-equality -Wno-parentheses -Wno-deprecated-declarations
  CCINCLUDE:=-Isrc -I$(MIDDIR) -I../romassist/src
  CCDECL:=-DPSS_STYLE=1 -DLSB_FIRST=1
  CC:=gcc -c -MMD -O2 $(CCINCLUDE) $(CCWARN) $(CCDECL)
  OBJC:=
  LD:=gcc
  LDPOST:=../romassist/out/libromassist.a -lz
  
  OPT:=romassist

  EXE:=$(OUTDIR)/akfceu
  TEST:=$(OUTDIR)/test
  
  RUNCMD:=$(EXE) ~/rom/nes/z/zelda.nes
endif

#------------------------------------------------------------------------------
# Generated source files.
# These must be located under $(MIDDIR), and you must provide a rule to generate them.

GENERATED_FILES:= \
  $(MIDDIR)/test/akfceu_test_contents.h

#------------------------------------------------------------------------------
# Everything below this point should work for any target.

SRCCFILES:=$(shell find src -name '*.[cm]')
MAINCFILES:=$(filter src/main/%,$(SRCCFILES))
TESTCFILES:=$(filter src/test/%,$(SRCCFILES))
CORECFILES:=$(filter-out $(MAINCFILES) $(TESTCFILES),$(SRCCFILES))

GENERATED_FILES:=$(GENERATED_FILES)
GENHFILES:=$(filter %.h,$(GENERATED_FILES))
GENCFILES:=$(filter %.c,$(GENERATED_FILES))
GENMAINCFILES:=$(filter $(MIDDIR)/main/%,$(GENCFILES))
GENTESTCFILES:=$(filter $(MIDDIR)/test/%,$(GENCFILES))
GENCORECFILES:=$(filter-out $(GENMAINCFILES) $(GENTESTCFILES),$(GENCFILES))

COREOFILES:=$(patsubst src/%,$(MIDDIR)/%.o,$(basename $(CORECFILES))) $(GENCOREOFILES:.c=.o)
TESTOFILES:=$(patsubst src/%,$(MIDDIR)/%.o,$(basename $(TESTCFILES))) $(GENTESTCFILES:.c=.o)
MAINOFILES:=$(patsubst src/%,$(MIDDIR)/%.o,$(basename $(MAINCFILES))) $(GENMAINCFILES:.c=.o)
ALLOFILES:=$(COREOFILES) $(TESTOFILES) $(MAINOFILES)
-include $(ALLOFILES:.o=.d)

$(MIDDIR)/%.o:src/%.c|$(GENHFILES);$(PRECMD) $(CC) -o $@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.c|$(GENHFILES);$(PRECMD) $(CC) -o $@ $<
$(MIDDIR)/%.o:src/%.m|$(GENHFILES);$(PRECMD) $(OBJC) -o $@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.m|$(GENHFILES);$(PRECMD) $(OBJC) -o $@ $<

OFILES_ITEST:=$(filter $(MIDDIR)/test/int/%,$(TESTOFILES))
OFILES_UTEST:=$(filter $(MIDDIR)/test/unit/%,$(TESTOFILES))
EXES_UTEST:=$(patsubst $(MIDDIR)/test/unit/%.o,$(OUTDIR)/utest/%,$(OFILES_UTEST))
$(OUTDIR)/utest/%:$(MIDDIR)/test/unit/%.o;$(PRECMD) $(LD) -o $@ $< $(LDPOST)

all:$(EXE) $(TEST) $(EXES_UTEST)
$(EXE):$(COREOFILES) $(MAINOFILES);$(PRECMD) $(LD) -o $@ $(COREOFILES) $(MAINOFILES) $(LDPOST)
$(TEST):$(COREOFILES) $(OFILES_ITEST);$(PRECMD) $(LD) -o $@ $(COREOFILES) $(OFILES_ITEST) $(LDPOST)

#------------------------------------------------------------------------------

run:$(EXE);$(RUNCMD)
test:$(TEST) $(EXES_UTEST);etc/tool/testrunner.sh $^
test-%:$(TEST) $(EXES_UTEST);AKFCEU_TEST_FILTER="$*" etc/tool/testrunner.sh $^

clean:;rm -rf mid out

$(MIDDIR)/test/akfceu_test_contents.h:etc/make/gentest.sh $(TESTCFILES) $(GENTESTCFILES);$(PRECMD) $^ $@

install:$(EXE);EXE="$(EXE)" UNAMES="$(UNAMES)" etc/tool/install.sh
