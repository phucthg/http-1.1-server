/*
	main_thread is used to accept incoming connection and epoll all the active connection
	if there is some event, a thread is dispatched for each event
	
*/
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <thread>

#include "http_socket.hpp"

class http_server{
	
protected:
	const int port, max_concurrent_connection, max_worker_thread;//the local port
	int server_fd, opt, address_length;
	struct sockaddr_in address;
	int concurrent_connection_count;
	http_socket sock[MAX_FD_VALUE];//each fd number will have its own socket object		
	int ep_fd;//fd for epolling
	struct epoll_event ep_event, events[MAX_FD_VALUE];//polling infastructures
	bool is_thread_fd[MAX_FD_VALUE];
	int thread_fd[MAX_FD_VALUE][2];
	int thread_id[MAX_FD_VALUE];
	int working_fd[MAX_FD_VALUE];
	int connection_pipe[2];
	std::vector <int> thread_pool;
	std::queue <int> fd_queue;
	std::queue <int> conn_fd_queue;
public:
	virtual int handle_request(http_socket& sock) = 0;//this function can be implemented to serve html (or other things)
	
	http_server(int port, int max_concurrent_connection = 10000, int max_worker_thread = 512): 
	port(port),	max_concurrent_connection(max_concurrent_connection), max_worker_thread(max_worker_thread), 
	concurrent_connection_count(0),	address_length(sizeof(address))	{
		for(int i = 0; i < MAX_FD_VALUE; i++){
			sock[i].set_fd(i);
		}
		ep_fd = epoll_create1(0);
		if(ep_fd < 0){
			perror("epoll fd create failed!\n");
			exit(-1);
		}
		
		//create the fd reserved for thread handling
		for(int i=0; i<max_worker_thread; i++){
			if(pipe2(thread_fd[i], O_NONBLOCK) < 0){
				perror("failed to create thread pipe");
				exit(-1);
			}
			is_thread_fd[thread_fd[i][PIPE_READ]]=1;
			is_thread_fd[thread_fd[i][PIPE_WRITE]]=1;
			std::cerr<<"Read-write pair: "<<thread_fd[i][PIPE_READ]<<' '<<thread_fd[i][PIPE_WRITE]<<'\n';
			thread_id[thread_fd[i][PIPE_READ]]=i;
			thread_id[thread_fd[i][PIPE_WRITE]]=i;
			thread_pool.push_back(i);
		}

	}
	
	~http_server(){
		close(ep_fd);
	}
	void start(){//start the server
		//necessary variable to use linux socket API
		server_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
		if(server_fd < 0){
			perror("socket create failed");
			exit(-1);
		}
		std::cerr<<"server_fd: "<<server_fd<<'\n';
		if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
			perror("socketopt failed");
			exit(-1);
		}
		
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(port);
		
		if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0){
			perror("bind failed");
			exit(-1);
		}
		
		if(listen(server_fd, 10000) < 0){//back log is set to 32, but doesn't really matter
			perror("listen");
			exit(-1);
		}
		
		if(pipe2(connection_pipe, O_NONBLOCK) < 0){
			perror("failed to create server connection pipe");
			exit(-1);
		}
		
		std::thread handler(&http_server::handle_connections, this);
		int new_fd;
		uint8_t val[2];
		while(true){
			new_fd = accept4(server_fd, (struct sockaddr *)&address, (socklen_t*)&address_length, SOCK_NONBLOCK);
			val[0] = new_fd & 255;
			val[1] = new_fd >> 8;
			//cerr<<new_fd<<'\n';
			write(connection_pipe[PIPE_WRITE], val, 2);
		}
	}
	
	
	void handle_connections(){//this function is ran on all the non-main thread
		//add the server_fd to polling list
		int current_size = 0;//the number of active fd in epoll
		ep_event.events = EPOLLIN;//trigger when there is data in
		ep_event.data.fd = connection_pipe[PIPE_READ];
		epoll_ctl(ep_fd, EPOLL_CTL_ADD, connection_pipe[PIPE_READ], &ep_event);
		current_size++;
		
		for(int i = 0; i < max_worker_thread; i++){//poll the read ends of thread handling pipes
			ep_event.data.fd = thread_fd[i][PIPE_READ];
			epoll_ctl(ep_fd, EPOLL_CTL_ADD, thread_fd[i][PIPE_READ], &ep_event);
			current_size++;
		}
		
		
		ep_event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;//edge-trigger and 1 shot for connections
		uint8_t buffer[8];
		int new_fd, event_count;
		//std::set <int> alive;
		while(true){//accept connection and monitor it in epoll
			//poll
			
			
			event_count = epoll_wait(ep_fd, events, current_size, 1);//poll for 100 ms 
			
			/*cerr<<"Concurrent: "<<concurrent_connection_count<<". Worker avaiable: "<<thread_pool.size()<<'\n';
			int temp=10;
			for(int x: alive){
				cerr<<x<<' ';
				temp--;
				if(temp==0) break;
			}
			cerr<<'\n';
			*/
			
			for(int i = 0; i < event_count; i++){//deal with events
				//cerr<<events[i].data.fd<<'\n';
				if(events[i].data.fd == connection_pipe[PIPE_READ]){
					int res = read(events[i].data.fd, buffer, 2); 
					if(res != 2){
						perror("connection threads protocol failed");
						exit(-1);
					}
					new_fd = (((int)buffer[1]) << 8) | buffer[0];
					conn_fd_queue.push(new_fd);
				}
				else if(is_thread_fd[events[i].data.fd]){//this should be a worker thread finishing
					//std::cerr<<"Thread return: "<<events[i].data.fd<<'\n';
					int pipe = thread_id[events[i].data.fd];
					thread_pool.push_back(pipe);
					
					int connection_fd = working_fd[pipe];
					int count = read(thread_fd[pipe][PIPE_READ], buffer, 8);
					//std:cerr<<"Buffer is: "<<int(buffer[0])<<'\n';
					if(count != 1){
						perror("threads protocol failed");
						exit(-1);
					}
					//thread can write back a single byte 0 or 1 to imply whether this fd should be removed or rearmed in polling service
					if(buffer[0]){//connection terminated, remove the fd
						concurrent_connection_count--;
						epoll_ctl(ep_fd, EPOLL_CTL_DEL, connection_fd, NULL);
						//alive.erase(connection_fd);
						close(connection_fd);//fd will not be closed outside of this place
						//cerr<<"Closed fd: "<<connection_fd<<'\n';
					}
					else{//rearm the fd
						ep_event.data.fd = connection_fd;
						epoll_ctl(ep_fd, EPOLL_CTL_MOD, connection_fd, &ep_event);
						current_size++;
					}
				}
				else{//this should be a connection recieving something
					//cerr<<"Data received: "<<events[i].data.fd<<'\n';
					//cerr<<events[i].events<<'\n';
					if(events[i].events & EPOLLERR){//error, just close this pipe and ignore this connection
						cerr<<"Error!\n";
						epoll_ctl(ep_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
						//alive.erase(events[i].data.fd);
						close(events[i].data.fd);//fd will not be closed outside of this place
					}
					else if(events[i].events & EPOLLHUP){
						cerr<<"Hanged up!\n";
						//alive.erase(events[i].data.fd);
						epoll_ctl(ep_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
						close(events[i].data.fd);//fd will not be closed outside of this place
					}
					else{
						fd_queue.push(events[i].data.fd);//enqueue to be used later
						current_size--;
					}
				}
			}
			
			while(concurrent_connection_count < max_concurrent_connection){//if the connection count is not maxed, accept this new connection
				if(conn_fd_queue.empty()) break;
				new_fd = conn_fd_queue.front();
				conn_fd_queue.pop();
				
				//cerr<<"Opened fd: "<<new_fd<<'\n';
				
				concurrent_connection_count++;
				//add the fd to epoll
				ep_event.data.fd = new_fd;
				//alive.insert(new_fd);
				epoll_ctl(ep_fd, EPOLL_CTL_ADD, new_fd, &ep_event);
				current_size++;
			}
			//assign connection to worker_thread
			while(!fd_queue.empty()){
				if(thread_pool.empty()) break;
				//cerr<<"Connection can be served!\n";
				
				int id = thread_pool.back();
				thread_pool.pop_back();
				int fd = fd_queue.front();
				fd_queue.pop();
				
				working_fd[id] = fd;
				std::thread worker(&http_server::handle_fd, this, id, fd);
				worker.detach();
			}
		}
	}
	
	void handle_fd(int id, int fd){
		//std::cerr<<"Handler: "<<id<<' '<<fd<<'\n';
		int res = sock[fd].receive_message();
		char buffer[2] = {0, 1};
		if(res < 0){//message is somehow incorrect, drop this connection
			write(thread_fd[id][PIPE_WRITE], buffer+1, 1);
		}
		else{
			res = handle_request(sock[fd]);
			if(res == 0){//handler successfully handled the reqest and want to keep the connection going
				write(thread_fd[id][PIPE_WRITE], buffer+0, 1);
			}
			else{//handler either refused to answer or want to terminate after answering
				write(thread_fd[id][PIPE_WRITE], buffer+1, 1);
			}
		}
	}
};