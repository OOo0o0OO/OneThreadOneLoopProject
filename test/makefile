all:http_server
http_server: http_server.cc http.hpp server.hpp
	g++ $^ -o $@ -g
clean:
	rm http_server