#include <bits/stdc++.h>
using namespace std;
#include "http_server.hpp"
#include "html_template.hpp"
html_template temp("test.html");
class chat_server: public http_server{
	using http_server::http_server;
	int handle_request(http_socket &sock){
		//sock.send_message
		http_response res;
		res.response="";
		//res.response=temp.render({"Title", to_string(sock.fd)});
		//cerr<<res.response<<'\n';
		sock.send_message(res);
		return 0;
	}
};
int main(int argc, char* argv[]){
	//cerr<<atoi(argv[1])<<'\n';
	chat_server cs(atoll(argv[1]), atoll(argv[2]), atoll(argv[3]));//port, cuncurrent connection cap, worker thread
	cs.start();
	//cerr<<"OK\n";
}