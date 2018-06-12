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
#include <time.h>

#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

#define BUFFER_SIZE 1024
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

static int
connect_socket(int connect_port, char *address)
{
    struct sockaddr_in a;
    int s;

   if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        close(s);
        return -1;
    }

   memset(&a, 0, sizeof(a));
    a.sin_port = htons(connect_port);
    a.sin_family = AF_INET;

   if (!inet_aton(address, (struct in_addr *) &a.sin_addr.s_addr)) {
        perror("bad IP address format");
        close(s);
        return -1;
    }

   if (connect(s, (struct sockaddr *) &a, sizeof(a)) == -1) {
        perror("connect()");
        shutdown(s, SHUT_RDWR);
        close(s);
        return -1;
    }
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
      client_fd[num_interfaces] = connect_socket(port, host);
      if (client_fd[num_interfaces] < 0) on_error("Could not create socket\n");
      int read = 1;
      send(client_fd[num_interfaces], buf, read, 0);
      printf("Client connected to %s:%d\n", ifa->ifa_name, port);
      num_interfaces++;
    }
  }
  
  int read = 0; 
  while (1) {
    int r, nfds = 0;
    fd_set rd, wr, er;
    clock_t t1, t2;
    t1=t2=0;
    FD_ZERO(&rd);
    FD_ZERO(&wr);
    FD_ZERO(&er);

    printf("sending ..\n");

    t1 = clock();    
    for(int a=0;a<num_interfaces;a++) {
      send(client_fd[a], buf, read, 0);
      if (err < 0) on_error("Client write failed\n");
    }
    read++;
    for(int a=0;a<num_interfaces;a++) {
      FD_SET(client_fd[a], &rd);
      nfds = max(nfds, client_fd[a]);
    }
    
    r = select(nfds + 1, &rd, &wr, &er, NULL);
    
    if (r == -1 && errno == EINTR)
      continue;
    
    if (r == -1) {
      perror("select()");
      exit(EXIT_FAILURE);
    }
    for(int a=0;a<num_interfaces;a++) {
      if (FD_ISSET(client_fd[a], &rd)) {
	int res = recv(client_fd[a], buf, BUFFER_SIZE, 0);
	if (!res) break; // done reading
	if (res < 0) on_error("Client read failed\n");
	if(t2==0)
	  t2=clock();
      }
    }
    printf("Got first answer in %fms\n", (((float)(t2-t1))/CLOCKS_PER_SEC)*1000.0);
  }
}
