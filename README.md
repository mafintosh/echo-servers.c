# echo-servers.c

A collection of various echo servers in c.
Mostly for my own amusement and education

To compile them clone this repo and do

```
make
./bin/tcp-echo-server 8000
```

Then in another terminal do

```
nc localhost 8000 # will connect to the echo server
```