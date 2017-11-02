#
# rss.cc -- RSS software calculation (drom DPDK) for testing
# Massimo Gallo
#
# Copyright (c) 2016 Nokia Bell Labs
#

CXX = g++
CXXLD = g++

CXXFLAGS = -Wall -std=gnu++11
LFLAGS = -Wall

ALLEXEC = rss 

OBJS  = rss.o

.cc.o:
	$(CXX) $(CXXFLAGS) -c $<

all: $(ALLEXEC)

rss: $(OBJS)
	$(CXXLD) $(LFLAGS) -o $@ $(OBJS)


clean:
	rm -f *.o $(ALLEXEC)
