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
CCWARN:=-Werror -Wimplicit -Wno-pointer-sign -Wno-parentheses-equality
CCINCLUDE:=-Isrc -I$(MIDDIR)
CCDECL:=-DHAVE_ASPRINTF=1 -DPSS_STYLE=1
CC:=gcc -c -MMD -O2 $(CCINCLUDE) $(CCWARN) $(CCDECL)
LD:=gcc
LDPOST:=-lz

# Main and unit-test executables.
EXE:=$(OUTDIR)/fceu
TEST:=$(OUTDIR)/test

# How to launch the main program.
RUNCMD:=$(EXE)

# Directories immediately under <src/opt> to include in the build.
OPT:=macos

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

SRCCFILES:=$(filter-out $(SRCOPTIGNORE),$(shell find src -name '*.c'))
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

COREOFILES:=$(patsubst src/%.c,$(MIDDIR)/%.o,$(CORECFILES)) $(GENCOREOFILES:.c=.o)
TESTOFILES:=$(patsubst src/%.c,$(MIDDIR)/%.o,$(TESTCFILES)) $(GENTESTCFILES:.c=.o)
MAINOFILES:=$(patsubst src/%.c,$(MIDDIR)/%.o,$(MAINCFILES)) $(GENMAINCFILES:.c=.o)
OPTOFILES:=$(patsubst src/%.c,$(MIDDIR)/%.o,$(OPTCFILES)) $(GENOPTCFILES:.c=.o)
ALLOFILES:=$(COREOFILES) $(TESTOFILES) $(MAINOFILES) $(OPTOFILES)
-include $(ALLOFILES:.o=.d)

$(MIDDIR)/%.o:src/%.c|$(GENHFILES);$(PRECMD) $(CC) -o $@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.c|$(GENHFILES);$(PRECMD) $(CC) -o $@ $<

all:$(EXE) $(TEST)
$(EXE):$(COREOFILES) $(MAINOFILES) $(OPTOFILES);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)
$(TEST):$(COREOFILES) $(TESTOFILES) $(OPTOFILES);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)

#------------------------------------------------------------------------------

run:$(EXE);$(RUNCMD)
test:$(TEST);$(TEST)

clean:;rm -rf mid out

$(MIDDIR)/test/akfceu_test_contents.h:etc/make/gentest.sh $(TESTCFILES) $(GENTESTCFILES);$(PRECMD) $^ $@
