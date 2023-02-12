//includes
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>

#define HTTP_10 0
#define HTTP_11 1

#define LISTEN_BACKLOG 10
#define PORTNO 9001

struct HTTPRequest {
  char filename[512];
  int version;
};


// called in main function before parsing
// check if it has a potential file name and a valid version #
// make sure it is a get request
int is_valid_request(char *req) {
  
}

// takes a valid request and parses it into a struct
struct HTTPRequest parse(char *req) {
  
}

int main() {

  // socket for listening for new connections
  int listensock;
  // address for listening socket to bind to
  struct sockaddr_in bindaddr;

  // clear bindaddr of any garbage data
  memset((void*)&bindaddr, 0, sizeof(bindaddr));

  // attempt to crete socket
  if ((listensock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "Failed to create listening socket\n");
    exit(1);
  }

  // initialize bindaddr
  bindaddr.sin_family = AF_INET;
  bindaddr.sin_port = htons(PORTNO); // its over nine thousaaaaand!
  bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  // attempt to bind listensock to a network address/port
  if (bind(listensock, (struct sockaddr*)&bindaddr, sizeof(bindaddr)) < 0) {
    close(listensock);
    fprintf(stderr, "Failed to bind socket\n");
    exit(1);
  }

  if (listen(listensock, LISTEN_BACKLOG) < 0) {
    // im not sure how to undo bind, i think we just need to close the file?
    close(listensock);
    fprintf(stderr, "Failed to bind socket. The port number is likely busy.\n");
    exit(1);
  }

  int newsock;
  struct sockaddr_in newsock_addr;
  socklen_t newsock_len = sizeof(newsock);
  memset((void*)&newsock, 0, sizeof(newsock));

  newsock = accept(listensock, (struct sockaddr*) &newsock, &newsock_len);
  if (newsock < 0) {
    close(listensock);
    fprintf(stderr, "Issue with accepting new connection.\n");
  }

  fprintf(stdout, "Successfully accepted connection\n");

  // close all sockets
  close(newsock);
  close(listensock);
  
  char *req;
  if (is_valid_request(req)) {
    struct HTTPRequest httpreq = parse(req);
  }

  return 0;
}
