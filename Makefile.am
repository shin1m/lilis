bin_PROGRAMS = lilis
AM_CPPFLAGS =
AM_CXXFLAGS = -std=c++17
AM_LDFLAGS =
LDADD = -lstdc++fs
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
lilis_SOURCES = lilis.cc

SUBDIRS = test
