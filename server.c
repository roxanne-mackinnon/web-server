// Basic HTTP web server
// Written by Jason Lee and Roxanne Mackinnon <copyright 2023>

// includes
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <fcntl.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <getopt.h>

// defines
#define HTTP_10 '0'
#define HTTP_11 '1'
#define LISTEN_BACKLOG 10
#define PORTNO 9001
#define MAX_CONNECTIONS 64
#define MAX_MSG_LENGTH 1024
#define BLOCKSIZE 4096
#define MAXTIME 10000


// structs and typedefs
struct HTTPRequest {
  char *filename;
  char version;
};

typedef enum HTTPCode {CODE_200, CODE_400, CODE_403, CODE_404} HTTPCode;

const char code_message[][32] =
  {
   [CODE_200] = "OK\n",
   [CODE_400] = "Bad Request\n",
   [CODE_403] = "Forbidden\n",
   [CODE_404] = "Not Found\n"
  };
const int code_value[] =
  {
   [CODE_200] = 200,
   [CODE_400] = 400,
   [CODE_403] = 403,
   [CODE_404] = 404
  };

typedef enum HTTPType {TEXT_PLAIN, TEXT_HTML, IMAGE_JPEG, IMAGE_GIF} HTTPType;
const char content_types[][32] =
  {
   [TEXT_HTML] = "text/html",
   [TEXT_PLAIN] = "text/plain",
   [IMAGE_JPEG] = "image/jpeg",
   [IMAGE_GIF] = "image/gif"
  };


struct HTTPHeaders {
  long content_length;
  HTTPCode code;  
  HTTPType type;
  char version;
};




// function signatures

/*
 * takes a valid request and parses it into a struct
 */
int parse(char *req, struct HTTPRequest *httpreq);

/*
 * Parse command-line arguments -document_root and -port
 */
int parse_arguments(int arc, char *argv[], char **root, int *portno);

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
 * Extract the HTTP mime type from the name of the file.
 */
HTTPType http_file_type(char *fname);

/*
 * Main procedure after fork()
 * client: file desciptor for client connection
 */
void serve_client(int client);


/*
 * Check if the server is allowed to serve the file descriptor
 * Return the size of the file in bytes, or -1 if not world-readable
 */
long check_access(int fd);


/*
 * Fulfill the HTTP request, keeping the connection open
 */
void fulfill_request(int client, char *req, char *version);

/*
 * Send content-type, content-length, and date headers
 */
void send_headers(int client, struct HTTPHeaders head);


/*
 * Send a file to the client in blocks
 */
void send_file(int client, int fd);



/* Main function
 * Create a web server that listens for HTTP requests and serves files.
 */
int main(int argc, char * argv[]) {

  // check program arguments
  char *document_root;
  int portno;
  if (argc != 5
      || (parse_arguments(argc, argv, &document_root, &portno) < 0)) {
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

  // when this counter reaches 256, clear any zombie processes.
  unsigned char counter = 0;
  // main server loop
  for(;;) {
    // get new connection
    int new_socket = accept_connection(listen_socket);
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
    if (counter == 255) {
      // wait any exited child process without hanging
      while (waitpid(-1, NULL, WNOHANG) > 0);
    }
    counter++;
  }

  close(listen_socket);
  

  return 0;
}


// function implementations

/*
 * Parse command-line arguments -document_root and -port
 */
int parse_arguments(int arc, char *argv[], char **root, int *portno) {
  // if there are five arguments, the only valid possibilities are
  // -document_root <filename> -port <portno>
  // and
  // -port <portno> -document_root <filename>

  // if first arg is -document_root and second is -port
  if (! (strcmp(argv[1], "-document_root") || strcmp(argv[3], "-port"))) {
    *root = argv[2];
    *portno = atoi(argv[4]);
    return 0;
  }
  else if (! (strcmp(argv[3], "-document_root") || strcmp(argv[1], "-port"))) {
    *root = argv[4];
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
  // for peace of mind
  memset((void*)httpreq, 0, sizeof(struct HTTPRequest));

  // parse HTTP request
  // should get it to grab the first line ONLY and disgard all headers
  int n = sscanf(req, "GET /%ms HTTP/1.%[01]", &(httpreq->filename), &(httpreq->version));
  if (n != 2) {
    free(httpreq->filename);
    return -1;
  }
  if (httpreq->version != HTTP_10 && httpreq->version != HTTP_11) {
    free(httpreq->filename);
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

  char version = 0;

  // poll for 10 seconds before first request
  struct pollfd poll_client = {.fd = client, .events = POLLIN, .revents = 0};
  if (poll(&poll_client, 1, 1000) <= 0) {
    shutdown(client, 0);
    close(client);
    exit(0);
  }

  // fulfillment loop
  while (recv(client, req, sizeof(req), 0) >= 0) {

    fulfill_request(client, req, &version);
    memset(req, 0, sizeof(req));
    if (version == HTTP_11) {
      if (poll(&poll_client, 1, 200) <= 0) {
	// timeout
	shutdown(client, 0);
	close(client);
	exit(0);
      }
      continue;
    }
    
    else {
      shutdown(client, 0);
      close(client);
      exit(0);
    }

  }

  shutdown(client, 0);
  close(client);
  exit(0);
}

/*
 *
 */

/*
 * Fulfill the HTTP request, keeping the connection open
 */
void fulfill_request(int client, char *req, char *version) {

  /*
   * headers: text/html, text/plain, image/jpeg, image/gif
   */

  // parse the request, send 400 response if invalid.
  struct HTTPRequest httpreq;
  struct HTTPHeaders httprsp;
  memset((void*)&httpreq, 0, sizeof(httpreq));
  memset((void*)&httprsp, 0, sizeof(httprsp));
  if(parse(req, &httpreq) < 0) {
    // send 400 error: bad request
    httprsp.code = CODE_400;
    httprsp.version = HTTP_10;
    send_headers(client, httprsp);
    return;
  }

  // set version so calling function has it
  *version = httpreq.version;
  httprsp.version = httpreq.version;
  httprsp.type = http_file_type(httpreq.filename);
  // open the file before stat-ing, so if it gets deleted we still have a reference.
  int fd;
  if (httpreq.filename != NULL) {
    fd = open(httpreq.filename, O_RDONLY);
    // filename is dynamically allocated by scanf() so free it now
    free(httpreq.filename);
  }
  else {
    httpreq.filename = "index.html";
    httprsp.type = TEXT_HTML;
  }

  // send 404 error, file not found
  if (errno == ENOENT) {
    httprsp.code = CODE_404;
    send_headers(client, httprsp);
    return;
  }

  // ensure the file is world-readable
  long fsize;
  if ((fsize = check_access(fd)) > 0) {
    // send 200 status, OK
    
    httprsp.code = CODE_200;
    httprsp.content_length = fsize;
    
    send_headers(client, httprsp);
    send_file(client, fd);
    return;
  }
  else {
    httprsp.code = CODE_403;
    httprsp.content_length = 0;
    send_headers(client, httprsp);
    return;
  }
}

/*
 * Extract the HTTP mime type from the name of the file.
 */
HTTPType http_file_type(char *fname) {
  char *s = strrchr(fname, '.');
  if (s == NULL) {
    return TEXT_PLAIN;
  }
  if (!strncmp(s, ".html", 5)) {
    return TEXT_HTML;
  }
  if (!strncmp(s, ".txt", 4)) {
    return TEXT_PLAIN;
  }
  if (!strncmp(s, ".jpeg", 5) || !strncmp(s, ".jpg", 5)) {
    return IMAGE_JPEG;
  }
  if (!strncmp(s, ".gif", 4)) {
    return IMAGE_GIF;
  }
}

/*
 * Send a file to the client in blocks
 */
void send_file(int client, int fd) {
  char buf[BLOCKSIZE];
  memset((void*)buf, 0, sizeof(buf));

  // send file loop
  int num_read = 0;
  while ((num_read = read(fd, buf, BLOCKSIZE)) > 0) {
    send(client, buf, num_read, 0);
    memset((void*)buf, 0, sizeof(buf));
  }

  close(fd);
}


/*
 * Send content-type, content-length, and date headers
 */
void send_headers(int client, struct HTTPHeaders head) {
  char buf[MAX_MSG_LENGTH];
  memset(buf, 0, sizeof(buf));
  sprintf(buf, "HTTP/1.%c %d %s",
	  head.version,
	  code_value[head.code],
	  code_message[head.code]);
  
  send(client, buf, strlen(buf), 0);

  time_t t = time(NULL);
  sprintf(buf,
	  "Content-Type: %s\n"
	  "Content-Length: %ld\n"
	  "Date: %s\n",
	  content_types[head.type],
	  head.content_length,
	  ctime(&t));
  send(client, buf, strlen(buf), 0);
}



/*
 * Check if the server is allowed to serve the file descriptor
 * Return the size of the file in bytes, or -1 if not world-readable
 */
long check_access(int fd) {

  // MUST ACCOUNT FOR .. UP DIRECTORY FILE
  struct stat pathstat;

  if (fstat(fd, &pathstat) < 0) {
    // something went wrong, cant access
    return -1;
  }

  // ensure the file is everyone-readable
  if (pathstat.st_mode & S_IROTH) {
    return pathstat.st_size;
  }

  return -1;
}
