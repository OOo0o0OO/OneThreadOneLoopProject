all:http_server
http_server: http_server.cc http.hpp server.hpp
	g++ $^ -o $@ -g
client06: client06.cc server.hpp
	g++ $^ -o $@ -g
client05: client05.cc server.hpp
	g++ $^ -o $@ -g
client04: client04.cc server.hpp
	g++ $^ -o $@ -g
client03: client03.cc server.hpp
	g++ $^ -o $@ -g
client02: client02.cc server.hpp
	g++ $^ -o $@ -g
client01: client01.cc server.hpp
	g++ $^ -o $@ -g
clean:
	rm http_server