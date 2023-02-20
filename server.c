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
#include <errno.h>
#include <err.h>
#include <getopt.h>

// defines
#define DEBUG 1
#define HTTP_10 '0'
#define HTTP_11 '1'
#define LISTEN_BACKLOG 10
#define PORTNO 9001
#define MAX_CONNECTIONS 64
#define MAX_MSG_LENGTH 512
#define MAX_FILENAME_LENGTH 512
#define BLOCKSIZE 4096


// structs and typedefs
struct HTTPRequest {
  char *filename;
  char version;
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
int parse(char *req, struct HTTPRequest *httpreq);

/*
 * Parse command-line arguments -document_root and -port
 */
int parse_arguments(int arc, char *argv[], char *root, int *portno);

/*
 * Procedure for setting up a listener socket
 * Returns positive listening socket file descriptor on success, -1 on failure.
 */
int setup_listener(int port_no, int backlog);

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
  char document_root[MAX_FILENAME_LENGTH];
  int portno;
  if (argc != 5
      || (parse_arguments(argc, argv, document_root, &portno) < 0)) {
    fprintf(stderr,
	    "usage: ./server [options]\n   options:\n"
	    "    -document_root   root directory for serving files\n"
	    "    -port            listening port for server\n"
	    );
    exit(1);
  }


  if (chdir(document_root) < 0) {
    fprintf(stderr, "Failed to move to document_root.\n");
    exit(1);
  }
  
  // get a socket for listening for new connections
  int listen_socket = setup_listener(portno, MAX_CONNECTIONS);
  if (listen_socket < 0) {
    fprintf(stderr, "Failed to setup listening socket. Port number may be busy.\n");
    exit(1);
  }

  // main server loop
  for(;;) {
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

/*
 * Parse command-line arguments -document_root and -port
 */
int parse_arguments(int arc, char *argv[], char *root, int *portno) {
  // if there are five arguments, the only valid possibilities are
  // -document_root <filename> -port <portno>
  // and
  // -port <portno> -document_root <filename>

  // if first arg is -document_root and second is -port
  if (! (strcmp(argv[1], "-document_root") || strcmp(argv[3], "-port"))) {
    strncpy(root, argv[2], MAX_FILENAME_LENGTH);
    *portno = atoi(argv[4]);
    return 0;
  }
  else if (! (strcmp(argv[3], "-document_root") || strcmp(argv[1], "-port"))) {
    strncpy(root, argv[4], MAX_FILENAME_LENGTH);
    *portno = atoi(argv[2]);
    return 0;
  }
  
  return -1;
}

/*
 * takes a valid request and parses it into a provided struct
 * returns 0 if request is well-formed, -1 otherwise
 */
int parse(char *req, struct HTTPRequest *httpreq) {
  // this is just a skeleton to keep the compiler happy
  
  int n = sscanf(req, "GET /%ms HTTP/1.%[01]", &(httpreq->filename), &(httpreq->version));
  if (n != 2) {
    return -1;
  }
  if (httpreq->version != HTTP_10 && httpreq->version != HTTP_11) {
    return -1;
  }
  
  return 0;
}

/*
 * Procedure for setting up a listener socket
 * Returns positive listening socket file descriptor on success, -1 on failure.
 */
int setup_listener(int port_no, int backlog) {

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
  bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);

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

  // if we read any bytes
  if (ret >= 0) {
    
    struct HTTPRequest httpreq;
    if(parse(req, &httpreq) < 0) {
      // send 400 error: bad request
      const char header[] = "HTTP/1.0 400 Bad Request\n";
      send(client, header, sizeof(header), 0);
      // i dont think we need to sync it... im not sure
      close(client);
      exit(1);
    }
    
    // open the file before stat-ing, so if it gets deleted we still have a reference.
    // CHANGE BACK TO httpreq.filename
    int fd;
    fd = open(httpreq.filename, O_RDONLY);
    if (errno == ENOENT) {
      // send 404 error, file not found
      const char header[] = "HTTP/1.0 404 Not Found\n";
      send(client, header, sizeof(header), 0);
      close(client);
      exit(1);
      
    }
    
    if (check_can_access(fd)) {
      // send 200 status, OK
      const char header[] = "HTTP/1.0 200 OK\n";
      send(client, header, sizeof(header), 0);

      send_file(client, fd);
    }
    else {
      // send 403 error, forbidden
      const char header[] = "HTTP/1.0 403 Forbidden\n";
      send(client, header, sizeof(header), 0);
      close(client);
      exit(1);

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
