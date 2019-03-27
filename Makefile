.RECIPEPREFIX +=

# For which architecture are we compiling? Default to host.
ARCH ?= $(shell uname -m | sed -e s/x86_64/x86/)

CC=gcc
CXX=g++

# Install directories
PREFIX?=/usr/local
INCLUDEDIR?=$(PREFIX)/include
LIBDIR?=$(PREFIX)/lib64
PKGLIBDIR?=$(LIBDIR)/pkgconfig

# Build settings
SRCDIR := src
ARCH_SRCDIR := src/$(ARCH)
INCLUDES := include src

# we'll need some architecture specific definitions
INCLUDES += src/$(ARCH)

# x86 will use xed for decode/encode of instructions
ifeq ($(ARCH),x86)
LIBS += libs/xed/obj/libxed.a
LIBS_INCLUDES += libs/xed/include/public/xed/
LIBS_INCLUDES += libs/xed/obj/
GEN_FILES += src/x86/gen/gen-defs.h
endif

GEN_CFLAGS = -O3 -MMD -MP -fPIC
GEN_CFLAGS += $(foreach dir,$(INCLUDES),-I$(dir))
GEN_CFLAGS += -Werror -Wall -Wextra
GEN_CFLAGS += -Wmissing-declarations -Wredundant-decls -Wpointer-arith
GEN_CFLAGS += -Wwrite-strings -Wswitch-default -Wmissing-include-dirs -Wundef

# generators don't rely on any libs
CFLAGS = $(GEN_CFLAGS) $(foreach dir,$(LIBS_INCLUDES),-I$(dir))

CONLYFLAGS := -std=gnu99
CXXFLAGS := -std=c++14

CSRC = $(wildcard $(SRCDIR)/*.c) $(wildcard $(SRCDIR)/util/*.c) $(wildcard $(ARCH_SRCDIR)/*.c)
CXXSRC = $(wildcard $(SRCDIR)/*.cpp) $(wildcard $(SRCDIR)/util/*.cpp) $(wildcard $(ARCH_SRCDIR)/*.cpp)
COBJ = $(CSRC:.c=.o)
CXXOBJ = $(CXXSRC:.cpp=.o)
DEP = $(CSRC:.c=.d) $(CXXSRC:.cpp=.d)

# Build targets

.PHONY: all
all: libdrob.so tests

.PHONY: debug
debug: CFLAGS += -DDEBUG -g
debug: all

libdrob.so: $(COBJ) $(CXXOBJ) $(LIBS)
    $(CXX) -shared $^ -o $@

# in order to have the include directories for xed, build it first
libs/xed/include/public/xed/: libs/xed/obj/libxed.a
libs/xed/obj/: libs/xed/obj/libxed.a
libs/xed/obj/libxed.a:
    git submodule update --init --recursive
    cd libs/xed; ./mfile.py --extra-flags "-fPIC" --no-encoder --limit-strings --no-amd --no-mpx

# x86 definition generator
src/x86/gen/gen-defs: src/x86/gen/gen-defs.o
src/x86/gen/gen-defs.o: src/x86/gen/gen-defs.cpp
    $(CXX) $(GEN_CFLAGS) $(CXXFLAGS) -o $@ -c $<
src/x86/gen/gen-defs.h: src/x86/gen/gen-defs
    ./src/x86/gen/gen-defs > $@

.PHONY: tests
tests: libdrob.so
    $(MAKE) -C tests

%.o: %.c $(LIBS_INCLUDES) $(GEN_FILES)
    $(CC) $(CFLAGS) $(CONLYFLAGS) -o $@ -c $<

%.o: %.cpp $(LIBS_INCLUDES) $(GEN_FILES)
    $(CXX) $(CFLAGS) $(CXXFLAGS) -o $@ -c $<

libdrob.pc:
    echo 'libdir=${DESTDIR}$(LIBDIR)' >> libdrob.pc
    echo 'includedir=$(DESTDIR)$(LIBDIR)' >> libdrob.pc
    cat libdrob.pc.in >> libdrob.pc

.PHONY: install
install: libdrob.so libdrob.pc
    mkdir -p $(DESTDIR)$(LIBDIR)
    cp libdrob.so $(DESTDIR)$(LIBDIR)/libdrob.so
    mkdir -p $(DESTDIR)$(INCLUDEDIR)
    cp include/drob.h $(DESTDIR)$(INCLUDEDIR)/drob.h
    mkdir -p $(DESTDIR)$(PKGLIBDIR)
    cp libdrob.pc $(DESTDIR)$(PKGLIBDIR)/libdrob.pc
    ldconfig $(DESTDIR)$(LIBDIR)

.PHONY: clean
clean:
    rm -f libdrob.so $(COBJ) $(CXXOBJ) $(DEP) $(GEN_FILES) libdrob.pc
    rm -f src/x86/gen/gen-defs.o src/x86/gen/gen-defs.d src/x86/gen/gen-defs
    $(MAKE) -C tests clean

-include $(DEP)
