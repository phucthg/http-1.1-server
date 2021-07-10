# http-1.1-server
This is a simple http 1.1 server framework in C++ (for linux).

The server rely on the use of epoll to handle connections as well as threads. The idea is heavily inspired by the author's past experience with flask.


#Buidling and running

A demo webserver is provided. To build it:

```
	g++ -Ofast -pthread gallery_server.cpp -o gallery_server.out
```

To run it:

```
	./gallery_server.out <port> <max_concurrent> <num_worker_thread>
```

Example:

```
	./gallery_server.out 1503 10000 8
```
The optimal number of max\_concurrent and num\_worker_thread depends on the machine.

In testing, the author had found that the server can handle at least ```10000``` concurrent connections at at least ```40000``` requests / second using ```httperf```. Using ```httperf``` on the same machine on an empty port give the connection limit of httperf to be around ```55000``` per second. 
