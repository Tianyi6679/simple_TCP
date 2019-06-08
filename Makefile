# CS118 - Spring 19 - Project 2
# Tyanyi Ma & Alexandre Tiard
#
CXX = g++
TARBALL = 804716580.tar.gz
CXXFLAGS = -g -std=c++11 -Wall -Wextra
TARGETS = client server
HEADERS = server.h tcpFunc.h
SOURCES = server.cpp tcpFunc.cpp client/client.cpp

default:
	@$(CXX) $(CXXFLAGS) -o $(TARGETS) $(SOURCES)

dist:
	tar cvzhf $(TARBALL) README Makefile $(SOURCES) $(HEADERS)

clean:
	-rm -f $(TARGET) $(OBJECT) $(TARBALL)
