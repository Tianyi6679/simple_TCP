#ifndef CS118_SERVER_H
#define CS118_SERVER_H
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <map>
#define MYPORT 5100
#define BACKLOG 10 
#endif 

extern int process_http(char* message);
