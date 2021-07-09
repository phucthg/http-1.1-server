/**
This file contains the http_message base class, the http_request class, and the http_response.

To serve a webpage, the server are expected to generate a http_response object based on each http_request object,
and send the data through a http_socket object (see http_socket).
*/

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <time.h>  
class http_message{
protected://shared fields
	std::map <std::string, std::string> headers;
	std::string content;
public://utilities
	
	std::vector <std::string> split(const std::string &s, const std::string &match, size_t limit = -1){
		//exact match for now
		//match is expected to be short enough that something like KMP will take longer to do
		std::vector <std::string> ans;
		ans.push_back("");
		for(int i = 0; i < s.size(); i++){
			if(ans.size() > limit){
				ans.back() += s[i];
			}
			else{
				bool good = true;
				if(i + match.size() <= s.size()){
					for(int j = 0; j < match.size(); j++){
						if(s[i + j] != match[j]){
							good = false;
							break;
						}
					}
				}
				if (good){
					ans.push_back("");
					i += match.size() - 1;
				}
				else{
					ans.back() += s[i];
				}
				
			}
		}
		if(ans.back() == ""){
			ans.pop_back();
		}
		return ans;
	}
	
	static std::string get_server_time(){
		//get time in RFC 822 / RFC 1123 format
		//example Mon, 15 Mar 1995 22:38:34 GMT
		time_t raw_time;
		struct tm *time_info;
		char buffer[29];
		time(&raw_time);
		time_info = gmtime(&raw_time);
		strftime(buffer, 80, "%a, %d %b %Y %X GMT", time_info);
		return buffer;
	}
	
};

class http_request: public http_message{
protected:
	std::string type;
	std::string uri;
public:
	
	http_request(){}
	void parse(const std::string &s){
		//cerr<<"Parsing: \r\n"<<s<<"\r\n________________________________________________________________________\r\n";
		//parse from a string
		
		//Status
		auto lines = split(s, "\r\n");
		auto tokens = split(lines[0], " ");
		type = tokens[0];
		uri = tokens[1];
		//string html_ver=tokens[2];//unused, only support http 1.1
		
		//Headers
		headers.clear();
		int content_start = -1;
		for(int i = 1; i < lines.size(); i++){
			tokens=split(lines[i], ": ", 1);
			if (tokens.empty()){//empty line. This is the end of the header
				content_start = i + 1;
				break;
			}
			else{
				headers[tokens[0]] = tokens[1];
			}
		}
		
		//Contents
		content = "";
		if(content_start != -1){
			//there is some content
			for(int i = content_start; i < lines.size(); i++){
				content += lines[i] + "\r\n";
			}
		}
	}
};

class http_response: public http_message{//only support html for now
public:
	std::string response;
	
	std::string get_HTTP() const{//get the HTTP raw to send back for an html file
		return "HTTP/1.1 200 OK\r\nConnection: Keep-Alive\r\nContent-Type: text/html; charset=UTF-8\r\nContent-Length: 0\r\n\r\n";
		std::string res = "";
		res += "HTTP/1.1 200 OK\r\n";
		res += "Connection: Keep-Alive\r\n";
		//res += "Date: " + get_server_time() + "\r\n";
		res += "Content-Type: text/html; charset=UTF-8\r\n";
		res += "Content-Length: " + std::to_string(response.size()) + "\r\n";
		res += "\r\n";
		res += response;
		return res;
	}
	
};