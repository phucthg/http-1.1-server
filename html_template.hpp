/*
This file contains the html_template class.

It is a helper for webpages allow generating html files with custom contents based on the request.

The template will be very basic for now:
	-Everything is kept the same.
	-EOLs will be replaced with \r\n
	-'%' are replaced by input strings in that order unless preceded by a '\'
	-'\' by itself must be represented as \\
*/
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
class html_template{
public:
	std::vector <std::string> components;
	
	html_template(const std::string &path): components(){//path should point to a .html file, but any text file should work
		std::ifstream input(path);
		std::string s;
		components.push_back("");
		while(getline(input, s)){
			components.back() += "\r\n";
			bool first_slash = 0;
			for(char c: s){
				if(first_slash){
					if(c == '\\'){
						components.back() += '\\';
					}
					else if(c == '%'){
						components.back() += '\%';
					}
					else{
						std::cerr << "\\" << c << " is not supported (yet)!\n";
						exit(-1);
					}
					first_slash = 0;
				}
				else if(c == '\\'){
					first_slash = 1;
				}
				else if(c == '%'){
					components.push_back("");
				}
				else{
					components.back() += c;
				}
			}
		}
		input.close();
	}
	
	std::string render(const std::vector <std::string> &params){
		std::string res = "";
		for(int i = 0; i < components.size(); i++){
			res += components[i];
			if(i + 1 < components.size()){
				res += params[i];
			}
		}
		return res;
	}
	
};
