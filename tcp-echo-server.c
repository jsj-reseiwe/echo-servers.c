#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

#define _GNU_SOURCE /* To get defns of NI_MAXSERV and NI_MAXHOST */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/if_link.h>
#include <string.h>
#include <errno.h>
#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

static int
listen_socket(int listen_port, unsigned long inaddr)
{
    struct sockaddr_in a;
    int s;
    int yes;

   if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }
    yes = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
            &yes, sizeof(yes)) == -1) {
        perror("setsockopt");
        close(s);
        return -1;
    }
    memset(&a, 0, sizeof(a));
    a.sin_port = htons(listen_port);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr =  inaddr;
    if (bind(s, (struct sockaddr *) &a, sizeof(a)) == -1) {
        perror("bind");
        close(s);
        return -1;
    }
    printf("accepting connections on port %d\n", listen_port);
    listen(s, 10);
    return s;
}

int main (int argc, char *argv[]) {
  if (argc < 2) on_error("Usage: %s [port]\n", argv[0]);

  int port = atoi(argv[1]);
  int server_fd[10], client_fd[10], err;
  struct sockaddr_in server[10], client[10];
  char buf[BUFFER_SIZE];

  struct ifaddrs *ifaddr, *ifa; 
  int family, s, n; 
  char host[NI_MAXHOST];
  int num_interfaces = 0;
  
  if (getifaddrs(&ifaddr) == -1) { 
    perror("getifaddrs"); 
    exit(EXIT_FAILURE); 
  }

  /* Walk through linked list, maintaining head pointer so we 
     can free list later */
  
  
  for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) { 
    if (ifa->ifa_addr == NULL) 
      continue;
    
    
    family = ifa->ifa_addr->sa_family;
    
    
    /* For an AF_INET* interface address, display the address */
    
    
    if (family == AF_INET && strncmp(ifa->ifa_name,"lo",2)) { 
      s = getnameinfo(ifa->ifa_addr, 
		      sizeof(struct sockaddr_in), 
		      host, NI_MAXHOST, 
		      NULL, 0, NI_NUMERICHOST); 
      if (s != 0) { 
                printf("getnameinfo() failed: %s\n", gai_strerror(s)); 
                exit(EXIT_FAILURE); 
      }
      unsigned long inaddr;
      inaddr = ((struct sockaddr_in *)(ifa->ifa_addr))->sin_addr.s_addr;
      server_fd[num_interfaces] = listen_socket(port, inaddr);
      /*socket(AF_INET, SOCK_STREAM, 0);
      if (server_fd < 0) on_error("Could not create socket\n");
      
      server[num_interfaces].sin_family = AF_INET;
      server[num_interfaces].sin_port = htons(port);   
      memcpy(&server[num_interfaces].sin_addr, &inaddr, sizeof(inaddr));

      int opt_val = 1;
      setsockopt(server_fd[num_interfaces], SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);
      
      err = bind(server_fd[num_interfaces], (struct sockaddr *) &server[num_interfaces], sizeof(server[0]));
      if (err < 0) on_error("Could not bind socket\n");
      
      err = listen(server_fd[num_interfaces], 128);
      if (err < 0) on_error("Could not listen on socket\n");
      */
      printf("Server is listening on %s:%d\n", ifa->ifa_name, port);
      num_interfaces++;
    }
  }
  int num_clients = 0;
  while (1) {
    int r, nfds = 0;
    fd_set rd, wr, er;
    
    FD_ZERO(&rd);
    FD_ZERO(&wr);
    FD_ZERO(&er);
    
    for(int a=0;a<num_interfaces;a++) {
      FD_SET(server_fd[a], &rd);
      FD_SET(server_fd[a], &wr);
      FD_SET(server_fd[a], &er);
      nfds = max(nfds, server_fd[a]);
    }
    for(int a=0;a<num_clients;a++) {
      FD_SET(client_fd[a], &rd);
      FD_SET(client_fd[a], &wr);
      FD_SET(client_fd[a], &er);
      nfds = max(nfds, client_fd[a]);
    }

    r = select(nfds + 1, &rd, &wr, &er, NULL);
    
    if (r == -1 && errno == EINTR)
      continue;
    
    if (r == -1) {
      perror("select()");
      exit(EXIT_FAILURE);
    }
    
    for(int a=0;a<num_interfaces;a++)
      if (FD_ISSET(server_fd[a], &rd)) {	
	socklen_t client_len = sizeof(client);
	client_fd[num_clients] = accept(server_fd[a], (struct sockaddr *) &client, &client_len);
	if (client_fd[num_clients] < 0) on_error("Could not establish new connection\n");
	printf("client number %d connected\n",num_clients );
	num_clients++;
      }

    for(int a=0;a<num_clients;a++)
      if (FD_ISSET(client_fd[a], &rd)) {
	int read = recv(client_fd[a], buf, BUFFER_SIZE, 0);
	if (!read) break; // done reading
	if (read < 0) on_error("Client read failed\n");
	
	for(int b=0;b<num_clients;b++) {
	  err = send(client_fd[b], buf, read, 0);
	  if (err < 0) on_error("Client write failed\n");
	}
      }
  }
  
  return 0;
}
