// Basic HTTP web server
// Written by Jason Lee and Roxanne Mackinnon <copyright 2023>

// includes
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <err.h>

// defines
#define DEBUG 1
#define HTTP_10 0
#define HTTP_11 1
#define LISTEN_BACKLOG 10
#define PORTNO 9001
#define MAX_CONNECTIONS 64
#define MAX_MSG_LENGTH 512
#define BLOCKSIZE 4096


// structs and typedefs
struct HTTPRequest {
  char filename[MAX_MSG_LENGTH];
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
 * Main procedure after fork()
 * client: file desciptor for client connection
 */
void serve_client(int client);

/*
 * Check if the server is allowed to serve the file descriptor
 */
int check_can_access(int fd);


/*
 * Send a file to the client in blocks
 */
void send_file(int client, int fd);




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


  // use getopt.h to get program arguments
  

  // get a socket for listening for new connections
  int listen_socket = setup_listener(PORTNO, INADDR_ANY, MAX_CONNECTIONS);
  if (listen_socket < 0) {
    fprintf(stderr, "Failed to setup listening socket. Port number may be busy.\n");
    exit(1);
  }

  printf("created listener\n");

  // main server loop
  while(1) {
    // get new connection
    int new_socket = accept_connection(listen_socket);
    printf("accepted\n");
    if (new_socket > 0) {

      // create a child process to serve the client
      int pid = fork();
      if (!pid) { // child
	serve_client(new_socket);
	exit(0);
      }
      else {      // parent
	close(new_socket);
      }
    }
  }

  close(listen_socket);
  

  return 0;
}


// function implementations

/* called in main function before parsing
 * check if it has a potential file name and a valid version #
 * make sure it is a get request
 */
int is_valid_request(char *req) {
  return 1;
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

  // attempt to create listener socket
  // use CLOEXEC so child doesn't have access to main listening socket
  if ((listen_sock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0)) < 0) {
    return -1;
  }

  // allow socket to reuse ports (taken from lecture 3)
  int optval = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
	     (const void *)&optval, sizeof(int));
    

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
  if (listen(listen_sock, LISTEN_BACKLOG) < 0) {
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
  socklen_t newsock_len = sizeof(newsock_addr);
  memset((void*)&newsock_addr, 0, sizeof(newsock_addr));

  printf("waiting\n");
  // block and wait for a new connection
  new_socket = accept(listen_socket, (struct sockaddr*) &newsock_addr, &newsock_len);

  // returns socket on success, or -1 if accept() failed
  return new_socket;
}

/*
 * Main procedure after fork()
 * client: file desciptor for client connection
 */
void serve_client(int client) {
  // skeleton
  // exit(0) is because it is a process

  // try using the message length header in HTTP request
  char req[MAX_MSG_LENGTH];
  memset((void*)req, 0, sizeof(req));

  // look into message length
  int ret = recv(client, req, sizeof(req), 0);
  
  if (ret >= 0 && is_valid_request(req)) {
    
    struct HTTPRequest httpreq = parse(req);

    // open the file before stat-ing, so if it gets deleted we still have a reference.
    // CHANGE BACK TO httpreq.filename
    int fd;
    if (DEBUG) {
      fd = open("test.txt", O_RDONLY);
    }
    else {
      fd = open(httpreq.filename, O_RDONLY);
    }
    
    
    
    if (check_can_access(fd)) {
      send_file(client, fd);
    }
    
    else {
      const char notfound[] = "HTTP/1.0 404 Not Found";
      send(client, notfound, sizeof(notfound), 0);
    }

    close(fd);
    close(client);
    exit(0);
  }

  // should send an error message instead of exit(1);

  close(client);
  exit(1);
}

/*
 * Send a file to the client in blocks
 */
void send_file(int client, int fd) {
  char buf[BLOCKSIZE];
  memset((void*)buf, 0, sizeof(buf));
  // write checks later

  const char header[] = "HTTP/1.0 200 OK\n";
  send(client, buf, sizeof(header), 0);

  // send file loop

  // it might be a good idea to check for errors and send an error
  // response if something gets interrupted midmessage
  int num_read = 0;
  while ((num_read = read(fd, buf, BLOCKSIZE)) > 0) {
    send(client, buf, num_read, 0);
    memset((void*)buf, 0, sizeof(buf));
  }
  
}


/*
 * Check if the server is allowed to serve the file descriptor
 */
int check_can_access(int fd) {

  // MUST ACCOUNT FOR .. UP DIRECTORY FILE
  struct stat pathstat;

  if (fstat(fd, &pathstat) < 0) {
    // something went wrong, cant access
    return 0;
  }

  // ensure the file is everyone-readable
  if (pathstat.st_mode & S_IROTH) {
    return 1;
  }

  return 0;
}
