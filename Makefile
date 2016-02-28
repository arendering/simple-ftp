all:
	g++ -std=c++11 server.cpp -o server -g
	g++ -std=c++11 client.cpp -o client -g
clean:
	rm server client

