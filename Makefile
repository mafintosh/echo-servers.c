all: bin/unix-echo-server bin/tcp-echo-server bin/tcp-non-blocking-echo-server bin/unix-non-blocking-echo-server

install: bin/unix-echo-server bin/tcp-echo-server
	cp bin/unix-echo-server /bin/
	cp bin/tcp-echo-server /bin/

bin/unix-echo-server: unix-echo-server.c
	cc unix-echo-server.c -o bin/unix-echo-server

bin/tcp-echo-server: tcp-echo-server.c
	cc tcp-echo-server.c -o bin/tcp-echo-server

bin/tcp-non-blocking-echo-server: tcp-non-blocking-echo-server.c
	cc tcp-non-blocking-echo-server.c -o bin/tcp-non-blocking-echo-server

bin/unix-non-blocking-echo-server: unix-non-blocking-echo-server.c
	cc unix-non-blocking-echo-server.c -o bin/unix-non-blocking-echo-server

clean:
	rm bin/*
