
#define HTTP_10 0
#define HTTP_11 1

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
  
  
  char *req;
  if (is_valid_request(req)) {
    struct HTTPRequest httpreq = parse(req);
  }
}
