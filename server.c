// includes
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>

// defines
#define HTTP_10 0
#define HTTP_11 1
#define LISTEN_BACKLOG 10
#define PORTNO 9001
#define MAX_CONNECTIONS 64

// structs and typedefs
struct HTTPRequest {
  char filename[512];
  int version;
};





// function signatures

/* called in main function before parsing
 * check if it has a potential file name and a valid version #
 * make sure it is a get request
 */
int is_valid_request(char *req);

/*
 * takes a valid request and parses it into a struct
 */
struct HTTPRequest parse(char *req);

/*
 * Procedure for setting up a listener socket
 * Returns positive listening socket file descriptor on success, -1 on failure.
 */
int setup_listener(int port_no, int ip_addr, int backlog);

/*
 * Wait for a new connection from a client
 * Returns the file descriptor of the new connection on success, or -1 on failure.
 */
int accept_connection(int listen_socket);

/*
 * Process the request and respond to the client. Intended for use after fork().
 * client_index: the index of the client connection in active_connections[].
 */
void serve_client(int client_index, struct HTTPRequest request);


/* Main function
 * Create a web server that listens for HTTP requests and serves files.
 */
int main(int argc, char * argv[]) {

  // check program arguments
  if (argc != 5) {
    fprintf(stderr,
	    "usage: ./server [options]\n   options:\n"
	    "    -document_root   root directory for serving files\n"
	    "    -port            listening port for server\n"
	    );
    exit(1);
  }

  
  // get a socket for listening for new connections
  int listen_socket = setup_listener(PORTNO, INADDR_ANY, MAX_CONNECTIONS);
  if (listen_socket < 0) {
    fprintf(stderr, "Failed to setup listening socket. Port number may be busy.\n");
    exit(1);
  }


  // wait for a client
  int new_socket = accept_connection(listen_socket);
  fprintf(stdout, "Successfully accepted connection\n");

  // close all sockets
  close(new_socket);
  close(listen_socket);
  
  char *req;
  if (is_valid_request(req)) {
    struct HTTPRequest httpreq = parse(req);
  }

  return 0;
}


// function implementations

/* called in main function before parsing
 * check if it has a potential file name and a valid version #
 * make sure it is a get request
 */
int is_valid_request(char *req) {
  return 0;
}

/*
 * takes a valid request and parses it into a struct
 */
struct HTTPRequest parse(char *req) {
  // this is just a skeleton to keep the compiler happy
  
  struct HTTPRequest result;
  memset((void*)&result, 0, sizeof(result));  
  return result;
}

/*
 * Procedure for setting up a listener socket
 * Returns positive listening socket file descriptor on success, -1 on failure.
 */
int setup_listener(int port_no, int ip_addr, int backlog) {

  // socket for listening for new connections
  int listen_sock;
  // address for listening socket to bind to
  struct sockaddr_in bindaddr;

  // clear bindaddr of any garbage data
  memset((void*)&bindaddr, 0, sizeof(bindaddr));

  // attempt to crete socket
  if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    return -1;
  }

  // initialize bindaddr
  bindaddr.sin_family = AF_INET;
  bindaddr.sin_port = htons(port_no);
  bindaddr.sin_addr.s_addr = htonl(ip_addr);

  // attempt to bind listensock to a network address/port
  if (bind(listen_sock, (struct sockaddr*)&bindaddr, sizeof(bindaddr)) < 0) {
    close(listen_sock);
    return -1;
  }

  // tell the OS to start listening for connections
  if (listen(listen_sock, backlog) < 0) {
    close(listen_sock);
    return -1;
  }

  return listen_sock;
}


/*
 * Wait for a new connection from a client
 * Returns the file descriptor of the new connection on success, or -1 on failure.
 */
int accept_connection(int listen_socket) {
  int new_socket;
  
  struct sockaddr_in newsock_addr;
  socklen_t newsock_len = sizeof(new_socket);
  memset((void*)&new_socket, 0, sizeof(new_socket));

  // block and wait for a new connection
  new_socket = accept(listen_socket, (struct sockaddr*) &new_socket, &newsock_len);

  // returns socket on success, or -1 if accept() failed
  return new_socket;
}

/*
 * Process the request and respond to the client. Intended for use after fork().
 * client_index: the index of the client connection in active_connections[].
 */
void serve_client(int client_index, struct HTTPRequest request) {
  // skeleton
  return;
}
