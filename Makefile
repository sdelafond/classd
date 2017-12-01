# Untangle Traffic Classification Daemon
# Copyright (c) 2011-2017 Untangle, Inc.
# All Rights Reserved
# Written by Michael A. Hotz

DEBUG = -g3 -ggdb
#GPROF = -pg
SPEED = -O2

BUILDID := "$(shell date -u "+%G/%m/%d %H:%M:%S UTC")"
VERSION := $(shell date -u "+%s")
SYSTEM := $(shell uname)
HFLOAT := $(shell cat /proc/cpuinfo | grep -m 1 vfp)
ARCH := $(shell uname -m)

ifeq ($(SYSTEM),Linux)
    PLATFORM = -D__LINUX__
    LIBFILES = -lpthread -ldl -lnavl
    ifeq ($(ARCH),x86_64)
        LIBPATH = -Lsrc/vineyard/lib64
    else ifneq (,$(findstring arm,$(ARCH)))
        ifneq (,$(findstring vfp,$(HFLOAT)))
            LIBPATH = -Lsrc/vineyard/libarmhf
	else
	    LIBPATH = -Lsrc/vineyard/libarm
	endif
    else
        LIBPATH = -Lsrc/vineyard/lib
    endif
else
  $(error ERROR: Unsupported platform '$(SYSTEM)')
endif

CXXFLAGS = $(DEBUG) $(GPROF) $(SPEED) -Wall -pthread

CXXFLAGS += -DVERSION=\"$(VERSION)\"
CXXFLAGS += -DBUILDID=\"$(BUILDID)\"
CXXFLAGS += -DPLATFORM=\"$(PLATFORM)\"

OBJFILES := $(patsubst src/%.cpp,src/%.o,$(wildcard src/*.cpp))

classd : $(OBJFILES)
	$(CXX) $(DEBUG) $(GPROF) $(SPEED) $(OBJFILES) $(LIBPATH) $(LIBFILES) -o classd

$(OBJFILES) : Makefile src/*.h

clean : force
	rm -f classd
	rm -f src/*.o

force :

