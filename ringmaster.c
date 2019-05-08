#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <unistd.h>
#include "potato.h"
int main(int argc, char *argv[])
{
  int status;
  int socket_fd;
  struct addrinfo host_info;
  struct addrinfo *host_info_list;
  int port;
  int num_players;
  int num_hops;
  const char *hostname = NULL;
  //struct hostent *host_addr;
  struct hostent *player_host;
  struct sockaddr_in player_addr;//used to accept player connection


  //check command line format
  if(argc != 4){
    printf("Usage: ./ringmaster <port_num> <num_players> <num_hops>\n");
    return EXIT_FAILURE;
  }
  port = atoi(argv[1]);
  num_players = atoi(argv[2]);
  num_hops = atoi(argv[3]);
  if(port <= 1024){
    printf("Port number can not be smaller than 1024.\n");
    return EXIT_FAILURE;
  }
  if(num_players <= 1){
    printf("Number of players must be greater than 1.\n");
    return EXIT_FAILURE;
  }
  if(num_hops < 0 || num_hops > 512){
    printf("Number of hops must be between 0 and 512.\n");
    return EXIT_FAILURE;
  }
  
  printf("Potato Ringmaster\n");
  printf("Players = %d\n",num_players);
  printf("Hops = %d\n",num_hops);
 
  memset(&host_info, 0, sizeof(host_info));

  host_info.ai_family   = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;
  host_info.ai_flags    = AI_PASSIVE;

  status = getaddrinfo(hostname, argv[1], &host_info, &host_info_list);
  //printf("hostname: %s\n",hostname);
  if (status != 0) {
    printf("Error: cannot get address info for host (%s, %d)\n",hostname,port);
    return EXIT_FAILURE;
  }

  socket_fd = socket(host_info_list->ai_family, 
		     host_info_list->ai_socktype, 
		     host_info_list->ai_protocol);
  if (socket_fd == -1) {
    printf("Error: cannot create socket (%s, %d)\n",hostname,port);
    return EXIT_FAILURE;
  }

  int yes = 1;
  status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  status = bind(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (status == -1) {
    printf("Error: cannot bind socket (%s, %d)\n",hostname,port);
    return EXIT_FAILURE;
  }

  status = listen(socket_fd, 100);
  if (status == -1) {
    printf("Error: cannot listen on socket (%s, %d)\n",hostname,port);
    return EXIT_FAILURE;
  }

  //printf("Waiting for connection on port %d\n", port);

  //store all player's socket file descriptor
  int player_socketfd[num_players];
  //store all player's info
  player player[num_players];
  
  for(int i = 0; i < num_players; ++i){
    
    socklen_t player_addr_len = sizeof(player_addr);
    player_socketfd[i] = accept(socket_fd, (struct sockaddr *)&player_addr, &player_addr_len);
    if(player_socketfd[i] < 0){
      printf("Error: cannot accept connection on socket.\n");
      return EXIT_FAILURE;
    }

    memset(&player[i],0,sizeof(player[i]));
    player[i].player_id = i;
    player[i].num_hops = num_hops;
    player[i].num_players = num_players;
    player[i].right_id = i + 1;
    player[i].left_id = i - 1;
    if(i == 0){
      player[i].left_id = num_players - 1;
    }
    else if(i == num_players - 1){
      player[i].right_id = 0;
    }

    //get player's host name and temporarily store
    player_host = gethostbyaddr((char*)&player_addr.sin_addr, sizeof(struct in_addr), AF_INET);
    strcpy(player[i].right_name, player_host->h_name);
    //printf("right name:%s\n",player[i].right_name);

    //receive player's port num and temporarily store
    ssize_t s = recv(player_socketfd[i],&player[i].right_port,sizeof(player[i].right_port),0);
    if (s == -1){
      perror("recv() right port");
      exit(EXIT_FAILURE);
    }
    
    
    
    //printf("right port: %d\n",player[i].right_port);
    
  }
  
  //rotate and store right neighbor's port and host name
  int tmp_port = player[0].right_port;
  char tmp_name[MAX_NAME];
  strcpy(tmp_name, player[0].right_name);
  
  for(int i = 0; i < num_players - 1; ++i){
    int right = player[i].right_id;
    player[i].right_port = player[right].right_port;
    strcpy(player[i].right_name, player[right].right_name);
  }
  player[num_players - 1].right_port = tmp_port;
  strcpy(player[num_players - 1].right_name, tmp_name);

  //send player info struct to player
  for(int i = 0; i < num_players; ++i){
    status = send(player_socketfd[i], &player[i], sizeof(player[i]), 0);
    if(status < 0){
      perror("send player struct");
      return EXIT_FAILURE;
    }
  }

  //receive ready signal from players
  //indicating already connected with neighbors
  for(int i = 0; i < num_players; ++i){
    int ready;
    ssize_t s = recv(player_socketfd[i], &ready, sizeof(ready), 0);
    if (s == -1){
      perror("recv() ready");
      exit(EXIT_FAILURE);
    }
    printf("Player %d is ready to play\n",i);
  }

  int shut_down = 0;
  //if hop is 0, immediately shut down the game
  if(num_hops == 0){
    shut_down = 1;
  }
  
  for(int i = 0; i < num_players; ++i){
    status = send(player_socketfd[i], &shut_down, sizeof(shut_down), 0);
    if(status < 0){
      perror("send shutdown");
      return EXIT_FAILURE;
    }
  }
  
  if(num_hops == 0){
    freeaddrinfo(host_info_list);
    close(socket_fd);
    for(int i = 0; i < num_players; ++i){
      close(player_socketfd[i]);
    }

    return EXIT_SUCCESS;
  }

  //start passing potato
  //sleep(1);

  //initialize potato
  potato potato;
  memset(&potato, 0, sizeof(potato));
  potato.num_hops = num_hops;
  potato.remain_hops = num_hops;

  srand((unsigned)time(NULL));
  int first_player = (rand()) % num_players;
  printf("Ready to start the game, sending potato to player %d\n",first_player);

  //send potato to first player
  status = send(player_socketfd[first_player], &potato, sizeof(potato), 0);
  if(status < 0){
    perror("send potato to first player");
    return EXIT_FAILURE;
  }

    
  //set up fd_set to monitor fd activity
  fd_set players_fd;
  int max_sd = player_socketfd[0];
  
  while(1){
    FD_ZERO(&players_fd);

    for (int i = 0; i < num_players; ++i) {
      FD_SET(player_socketfd[i], &players_fd);
      if (player_socketfd[i] > max_sd) {
	max_sd = player_socketfd[i];
      }
    }

    if(select(max_sd + 1, &players_fd, NULL, NULL, NULL) < 0){
      perror("select players");
      return EXIT_FAILURE;
    }

    for(int i = 0; i < num_players; ++i){

      //activity at any player socket indicate the end of game
      if(FD_ISSET(player_socketfd[i], &players_fd)){

	//receive potato
	memset(&potato, 0, sizeof(potato));
	ssize_t s = recv(player_socketfd[i], &potato, sizeof(potato), 0);
	if (s == -1){
	  perror("recv() last potato");
	  exit(EXIT_FAILURE);
	}

	//send shutdown signal to all players
	for(int i = 0; i < num_players; ++i){
	  int shutdownstat = shutdown(player_socketfd[i],SHUT_RDWR);
	  if(shutdownstat < 0){
	    printf("shutdown at player %d\n",i);
	    return EXIT_FAILURE;
	  }
	}

	//print trace of player id
	printf("Trace of potato:\n");
	for(int i = 0; i < num_hops; ++i){
	  if(i == num_hops - 1){
	    printf("%d\n",potato.trace[i]);
	  }
	  else{
	    printf("%d,",potato.trace[i]);
	  }
	}
	
	freeaddrinfo(host_info_list);
	close(socket_fd);
	for(int i = 0; i < num_players; ++i){
	  close(player_socketfd[i]);
	}
	return EXIT_SUCCESS;
	
      }//if FD_ISSET

      
    }//for loop

    
  }//while loop
  

  return EXIT_SUCCESS;
}
