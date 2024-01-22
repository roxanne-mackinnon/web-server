web-server
======

A web server compliant with HTTP/1.0 and HTTP/1.1 capable of serving static content. web-server works well with browsers and is designed to support many connections concurrently with minimal performance degradation.

Running web-server
======
To compile web-server, simply run:
```
make server
```

To run, provide a folder with files to serve and a port to listen at for new connections. For example, to serve files out of the directory 'site' on port 8080, simply run:

```
./server -document_root site -port 8080
```
