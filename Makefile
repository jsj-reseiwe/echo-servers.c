all: bin/tcp-echo-server bin/tcp-echo-client
install: bin/tcp-echo-server
	cp bin/tcp-echo-server /bin/

bin/tcp-echo-server: tcp-echo-server.c
	cc -g tcp-echo-server.c -o bin/tcp-echo-server

bin/tcp-echo-client: tcp-echo-client.c
	cc -g tcp-echo-client.c -o bin/tcp-echo-client

clean:
	rm bin/*
