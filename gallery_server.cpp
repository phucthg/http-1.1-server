/**
This file implements a demo webpage for the http server.
It is a simple webpage that serve all the images inside the image folder, with the ability to add more image by posting a link of that image.
This weird way of adding image is due to the server not supporting multipart read (yet).

More precisely:
	The server support GET request for images and html pages
	The server support POST request for texts, which will be interpreted as image link, download and saved to disk. 
*/
#include <bits/stdc++.h>
using namespace std;
#include "http_server.hpp"
#include "html_template.hpp"
html_template gallery_template("./template/gallery_template.html");
html_template image_embed("./template/image_embed.html");
class gallery_server: public http_server{
public:
	using http_server::http_server;
	map <string, string> imgs;//store the image as a string containing binary data
	
	void load_image(const string &s){
		if(imgs.find(s) != imgs.end()){
			return;
		}
		string res;
		ifstream imgfile("./image/" + s, ios::binary);
		std::vector<char> buffer((std::istreambuf_iterator<char>(imgfile)), (std::istreambuf_iterator<char>()));
		for(char &c: buffer){
			res += c;
		}
		imgs[s] = res;
	}
	
	void load_images(){
		system("ls image -1 > img_list.out");
		ifstream f("img_list.out");
		string s;
		while(getline(f, s)){
			load_image(s);
		}
	}
	
	string render_gallery(){
		string res = "";
		for(auto &x: imgs){
			res = image_embed.render({res, "image/" + x.first});
		}
		return gallery_template.render({res});
	}
	
	vector <string> parse_uri(const string &uri){
		int i;
		string category = "";
		string resource = "";
		for(i = 1; i < uri.size(); i++){
			if(uri[i] == '/'){
				break;
			}
			category += uri[i];
		}
		for(i++; i < uri.size(); i++){
			if(uri[i] == '/'){
				break;
			}
			resource += uri[i];
		}
		return {category, resource};
	}
	
	int handle_request(http_socket &sock){
		http_response res;
		if(sock.request.type == "GET"){
			auto info = parse_uri(sock.request.uri);
			if(info[0] == "image"){
				if(imgs.find(info[1]) == imgs.end()){
					res.status_code = "404";
					res.reason_phrase = "Not found";
				}
				else{
					res.status_code = "200";
					res.reason_phrase = "OK";
					res.headers["Cache-Control"] = "public, max-age=604800, immutable";
					res.headers["Content-Type"] = "image";
					res.content = imgs[info[1]];
				}
			}
			else if(info[0] == "home"){
				//show all the image 
				res.content = render_gallery();
			}
			else{//404
				res.status_code = "404";
				res.reason_phrase = "Not found";
			}
		}
		else if(sock.request.type == "POST"){
			auto info = parse_uri(sock.request.uri);
			if(info[0] == "home"){
				bool valid = true;
				string link, extension, file_name;
				link = sock.request.content.substr(0, sock.request.content.size() - 2);//\r\n\r\n 
				if(link.size() <= 9){//5 for link= and 4 for extension
					valid = false;
				}
				if(valid){
					link = link.substr(5);
					extension = link.substr(link.size() - 4);
					if((extension != ".jpg") && (extension != ".png")){
						valid = false;
					}
				}
				if(valid){
					for(int i = link.size() - 1; i >= 0; i--){
						if(link[i] == '/'){
							break;
						}
						file_name += link[i];
					}
					reverse(file_name.begin(), file_name.end());
					if(file_name == link){
						valid = false;
					}
					else if(imgs.find(file_name) != imgs.end()){
						valid = false;
					}
				}
				if(valid){
					system(("curl " + link + " --output image/" + file_name).c_str());
					load_image(file_name);
					res.content = render_gallery();
				}
				else{
					res.status_code = "400";
					res.reason_phrase = "Bad request";
				}
			}
			else{
				res.status_code = "404";
				res.reason_phrase = "Not found";
			}
		}
		else{
			res.status_code = "501";
			res.reason_phrase = "Not implemented";
		}
		sock.send_message(res);
		return 0;
	}
};

int main(int argc, char* argv[]){
	if(argc != 4){
		cerr << "Provide the port, the concurrent connection cap, and the number of worker thread!\n";
		return -1;
	}
	gallery_server cs(atoll(argv[1]), atoll(argv[2]), atoll(argv[3]));//port, cuncurrent connection cap, worker thread
	cs.load_images();
	cs.start();
}