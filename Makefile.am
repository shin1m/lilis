bin_PROGRAMS = slisp
AM_CPPFLAGS =
AM_CXXFLAGS = -std=c++17
AM_LDFLAGS =
if DEBUG
AM_CXXFLAGS += -O0 -g
else
AM_CPPFLAGS += -DNDEBUG
AM_CXXFLAGS += -O3
endif
if PROFILE
AM_CXXFLAGS += -pg
AM_LDFLAGS += -pg
endif
slisp_SOURCES = slisp.cc

SUBDIRS = test
