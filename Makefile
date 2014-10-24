all: bin/unix-echo-server bin/tcp-echo-server

bin/unix-echo-server: unix-echo-server.c
	cc unix-echo-server.c -o bin/unix-echo-server

bin/tcp-echo-server: tcp-echo-server.c
	cc tcp-echo-server.c -o bin/tcp-echo-server

clean:
	rm bin/*
