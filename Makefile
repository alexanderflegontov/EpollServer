
all:	clean server client

print_ps:
	ps -a
print_syslog:
	tail /var/log/syslog | grep client
print_daemons:
	ps -eo 'tty,pid,comm' | grep ^? | grep client

server:
	g++ server_main.cpp server.cpp connection.cpp read_json.cpp -lboost_json -std=c++17 -o server
client:
	g++ client_main.cpp client.cpp connection.cpp read_json.cpp -lboost_json -std=c++17 -o client

clean:
	rm -f server client

