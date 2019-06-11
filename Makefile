# CS118 - Spring 19 - Project 2
# Tyanyi Ma & Alexandre Tiard
#
CXX = g++
TARBALL = 804716580.tar.gz
CXXFLAGS = -g -std=c++11 
SERVER = server
CLIENT = client
objects = server client 
S_H = server.h tcpFunc.h
SOURCE1 = server.cpp tcpFunc.cpp 
SOURCE2 = client_dir/client.cpp tcpFunc.cpp

all: $(objects)

$(SERVER):server.o tcpFunc.o 
	$(CXX) $(CXXFLAGS) server.o tcpFunc.o -o $(SERVER)

$(CLIENT):client.o tcpFunc.o
	$(CXX) $(CXXFLAGS) client.o tcpFunc.o -o $(CLIENT) 

server.o:server.cpp 
	$(CXX) -c server.cpp

client.o:client_dir/client.cpp
	$(CXX) -c client_dir/client.cpp

tcpFunc.o:tcpFunc.cpp
	$(CXX) -c tcpFunc.cpp

dist:
	tar cvzhf $(TARBALL) README.md Makefile server.cpp $(SOURCE2) $(S_H)

clean:
	-rm -f $(OBJECT) $(TARBALL)
