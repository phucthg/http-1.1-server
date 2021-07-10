/*
This file contains the http_socket class.

This class will handle the recv function, more precisely, it will read until the end of a http_message, parse that message into a http_request object.
This class will handle the send function, more precisely, it will send a http_message in the form of a http_respond object.

The user is expected to implement a function that would handle a http_socket inside http_server:
	This function will be called to run on its own thread when there is something to read (epoll hit).
	After the function has responded to this connection, it can choose to:
		Keep the connection to handle manually,
		Close the connection, or
		Place the connection back into the connection pool.

Each connection will be handled using only one http_socket object.
The object will still live even after the fd is closed and will be reused.
The number of fd can be defined in teh http_define.hpp file

*/
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <poll.h>
#include "http_define.hpp"
#include "http_message.hpp"

class http_socket{
public:
	int fd;
	http_request request;
	int buffer_size;
	char *buffer;
	struct pollfd *pfds;
	http_socket(): fd(), request(), buffer_size(SOCKET_STARTING_BUFFER_SIZE){
		buffer = new char[buffer_size];;
		pfds = new struct pollfd[1];
		pfds[0].events = POLLIN | POLLOUT;
		//if buffer_size is reached, it is doubled in size. This should happen fairly rarely, and if it happen often enough then change SOCKET_STARTING_BUFFER_SIZE
	}
	void set_fd(int fd){
		this->fd = fd;
		pfds[0].fd = fd;
	}
	
	
	int receive_message(){
		//Read a message and create a http_request object based on that message.
		//This function make sure that it reads until the end of a message, even if it mean blocking or waiting
		//I.E. No partial read, though that might be beneficial
		//Return -1 if there is any error (errno would be set be the error)
		//Return 0 if a message is read (and no EOF is found).
		//Return 1 is EOF is found (pipe closed)
		int return_value = 0;
		int checked = 3;
		size_t mss_size = 0;
		size_t expected_size = -1;
		size_t body_start = 0;
		while(mss_size < expected_size){
			//entering this loop mean that there's something to do
			int read_size = read(fd, buffer + mss_size, buffer_size - mss_size);
			if(read_size < 0){
				if(errno == EAGAIN){//if thereis nothing to read, wait
					//poll unitl the read end is ready.
					//idealistically we can put this back to the epolling thread, but we will poll here for now
					if(poll(pfds, 1, 30000) < 0){//timed out or error, just drop this connection
						perror("polling");
						exit(-1);
					}
					//there is something to read so the loop is restarted
				}
				else{
					return -1;
				}
			}
			else if(read_size == 0){
				//fd closed, can be valid if this is a post request
				return_value = 1;
				break;
			}
			else{
				mss_size += read_size;
				if(mss_size == buffer_size){//when the message reach the limit of the buffer, expand it.
					buffer_size *= 2;
					char *temp = buffer;
					buffer = new char[buffer_size];
					memcpy(buffer, temp, mss_size);
					delete[] temp;
				}
				
				if(mss_size == expected_size){//done reading
					break;
				}
				else if(expected_size == -1){//check for end of headers
					for(; checked < mss_size; checked++){
						if(buffer[checked] != '\n'){
							continue;
						}
						if(buffer[checked - 1] != '\r'){
							continue;
						}
						if(buffer[checked - 2] != '\n'){
							continue;
						}
						if(buffer[checked - 3] != '\r'){
							continue;
						}
						//this is the end of header
						body_start = checked + 1;
						break;
					}
					if(body_start){//headers is done
						expected_size = 0;
						char* found = strstr(buffer, "Content-Length: ");//search the string for Content-Length
						if(found != NULL){//request has a body
							int pos = found - buffer;
							pos += 16;
							while(isdigit(pos)){
								(expected_size *= 10) += buffer[pos];
								pos++;
							}
							expected_size += body_start;
						}
						else{
							break;//no body
						}
					}
				}
			}
		}
		if(mss_size){
			buffer[mss_size] = 0;//nullterminating
			request.parse(buffer);
		}
		else{
			return -1;
		}
		return return_value;
	}
	
	int send_message(const std::string &content){//text/html only for now
		int res = 0;
		while(res < content.size()){//make sure to send everything, a poll would be nice but it would not happen for most request 
			int new_byte = send(fd, &content[res], content.size() - res, 0);
			if(new_byte == -1){
				continue;
			}
			res += new_byte;
		}
		return res;
	}
	
	int send_message(http_response &response){//text/html only for now
		return send_message(response.get_HTTP());
	}

};
