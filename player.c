#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include "potato.h"

int main(int argc, char *argv[])
{
  int status;
  int master_socket_fd;
  struct addrinfo host_info;
  struct addrinfo *host_info_list;
  struct addrinfo player_info;
  struct addrinfo *player_info_list;
  struct addrinfo right_info;
  struct addrinfo *right_info_list;
  int master_port;
  struct hostent *master_addr;
  //struct hostent *player_host;
  int player_socket_fd;
  const char *playername = NULL;
  int right_socket_fd;
  int left_socket_fd;
  
  if (argc != 3) {
    printf("Usage: ./player <machine_name> <port_num>\n");
    return EXIT_FAILURE;
  }
  
  master_port = atoi(argv[2]);
  if(master_port <= 1024){
    printf("Port number can not be smaller than 1024.\n");
    return EXIT_FAILURE;
  }

  if ((master_addr = gethostbyname(argv[1])) == NULL){
    perror("gethostbyname");
    return EXIT_FAILURE;
  }
  
  
  /************connect to ringmaster***************/
  memset(&host_info, 0, sizeof(host_info));
  host_info.ai_family   = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;

  status = getaddrinfo(argv[1], argv[2], &host_info, &host_info_list);
  if (status != 0) {
    printf("Error: cannot get address info for host (%s, %d)\n",argv[1],master_port);
    return EXIT_FAILURE;
  } 

  master_socket_fd = socket(host_info_list->ai_family, 
		     host_info_list->ai_socktype, 
		     host_info_list->ai_protocol);
  if (master_socket_fd == -1) {
    printf("Error: cannot create socket for host (%s, %d)\n",argv[1],master_port);
    return EXIT_FAILURE;
  } 
  
 
  status = connect(master_socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (status == -1) {
    printf("Error: cannot connect socket for host (%s, %d)\n",argv[1],master_port);
    return EXIT_FAILURE;
  }

  /************create player socket and wait for connection***************/
  
  char player_port[10];

  for(int i = 50000; i <= 50500; ++i){

    memset(&player_info, 0, sizeof(player_info));

    player_info.ai_family   = AF_UNSPEC;
    player_info.ai_socktype = SOCK_STREAM;
    player_info.ai_flags    = AI_PASSIVE;
    char port[10];
    sprintf(port,"%d",i);
    
    status = getaddrinfo(playername, port, &player_info, &player_info_list);
    if (status != 0) {
      printf("Error: cannot get address info for host (%s, %d)\n",playername,i);
      return EXIT_FAILURE;
    }

    player_socket_fd = socket(player_info_list->ai_family, 
			      player_info_list->ai_socktype, 
			      player_info_list->ai_protocol);
    if (player_socket_fd == -1) {
      printf("Error: cannot create socket (%s, %d)\n",playername,i);
      return EXIT_FAILURE;
    }

    int yes = 1;
    status = setsockopt(player_socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    status = bind(player_socket_fd, player_info_list->ai_addr, player_info_list->ai_addrlen);
    if (status == -1) {
      if(i == 50500){
	printf("Error: No available port to bind\n");
	freeaddrinfo(host_info_list);
	freeaddrinfo(player_info_list);
	return EXIT_FAILURE;
      }
      //printf("player hostname, port:%s, %s\n",playername,port);
      freeaddrinfo(player_info_list);
      continue;
    }

    status = listen(player_socket_fd, 100);
    if (status == -1) {
      printf("Error: cannot listen on socket (%s, %d)\n",playername,i);
      return EXIT_FAILURE;
    }
    else if (status == 0){
      sprintf(player_port,"%d",i);
      break;
    }  
  }//find available port for loop

  
  
  //send player port number to ring master
  int port = atoi(player_port);
  send(master_socket_fd, &port, sizeof(port), 0);

  //receive player struct
  player player;
  recv(master_socket_fd, &player, sizeof(player), 0);

  printf("Connected as player %d out of %d total players\n",player.player_id,player.num_players);
  //printf("right port: %d\n",player.right_port);

  /**First connect to right player then accept connection to left player**/
  memset(&right_info, 0, sizeof(right_info));
  right_info.ai_family   = AF_UNSPEC;
  right_info.ai_socktype = SOCK_STREAM;
  char right_port[10];
  sprintf(right_port,"%d",player.right_port);
  //printf("right name: %s\n",player.right_name);

  status = getaddrinfo(player.right_name, right_port, &right_info, &right_info_list);
  if (status != 0) {
    printf("Error: cannot get address info for right (%s, %d)\n",player.right_name,player.right_port);
    return EXIT_FAILURE;
  }
  
  right_socket_fd = socket(right_info_list->ai_family, 
		     right_info_list->ai_socktype, 
		     right_info_list->ai_protocol);
  if (right_socket_fd == -1) {
    printf("Error: cannot create socket for right (%s, %d)\n",player.right_name,player.right_port);
    return EXIT_FAILURE;
  } 
  
  //connect to right neighbor's player socket
  status = connect(right_socket_fd, right_info_list->ai_addr, right_info_list->ai_addrlen);
  if (status == -1) {
    printf("Error: cannot connect socket for right (%s, %d)\n",player.right_name,player.right_port);
    return EXIT_FAILURE;
  }

  //accept connection from left neighbor
  struct sockaddr_storage left_socket_addr;
  socklen_t left_socket_addr_len = sizeof(left_socket_addr);
  left_socket_fd = accept(player_socket_fd, (struct sockaddr *)&left_socket_addr, &left_socket_addr_len);
  if(left_socket_fd < 0){
    printf("Error: cannot accept connection on socket.\n");
    return EXIT_FAILURE;
  }
  //printf("connected with left and right neighbor\n");

  int ready = 0;
  status = send(master_socket_fd, &ready, sizeof(ready), 0);
  if(status < 0){
      perror("send player ready");
      return EXIT_FAILURE;
    }

  //receive signal and shut down/continue game
  int shut_down;
  recv(master_socket_fd, &shut_down, sizeof(shut_down), 0);
  if(shut_down){
    freeaddrinfo(host_info_list);
    freeaddrinfo(player_info_list);
    freeaddrinfo(right_info_list);
    close(right_socket_fd);
    close(left_socket_fd);
    close(master_socket_fd);
    close(player_socket_fd);
    return EXIT_SUCCESS;
  }

  //printf("hop is not 0\n");


  /**Start processing potato**/
  fd_set set_fd;
  int main_fd;
  int max_sd = master_socket_fd;
  srand(time(NULL) + player.player_id);

  while(1){
    FD_ZERO(&set_fd);
    FD_SET(master_socket_fd, &set_fd);
    FD_SET(right_socket_fd, &set_fd);
    if(right_socket_fd > max_sd){
      max_sd = right_socket_fd;
    }
    FD_SET(left_socket_fd, &set_fd);
    if(left_socket_fd > max_sd){
      max_sd = left_socket_fd;
    }

    if(select(max_sd + 1, &set_fd, NULL, NULL, NULL) < 0){
      perror("select");
      return EXIT_FAILURE;
    }

    //monitor fd activity and see where to receive the potato
    if (FD_ISSET(master_socket_fd, &set_fd)){
      main_fd = master_socket_fd;
      //printf("Received from ringmaster\n");
    }
    else if (FD_ISSET(left_socket_fd, &set_fd)){
      main_fd = left_socket_fd;
      //printf("Received from left player %d\n", player.left_id);
    }
    else if (FD_ISSET(right_socket_fd, &set_fd)){
      main_fd = right_socket_fd;
      //printf("Received from right player %d\n", player.right_id);
    }
    else{
      printf("Unknown fd error\n");
      return EXIT_FAILURE;
    }

    potato recv_potato;
    memset(&recv_potato, 0, sizeof(recv_potato));
    ssize_t s = recv(main_fd, &recv_potato, sizeof(recv_potato), 0);
    if (s == -1){
      perror("recv() potato");
      exit(EXIT_FAILURE);
    }
    if(s == 0){//connection is closed
      //printf("connection closed by ring master\n");
      break;
    }
    //printf("Potato info: remaining hops: %d\n",recv_potato.remain_hops);

    
    //edit potato info
    int trace_index = recv_potato.num_hops - recv_potato.remain_hops;
    recv_potato.trace[trace_index] = player.player_id;
    recv_potato.remain_hops--;

    //sleep(1);

    if(recv_potato.remain_hops <= 0){

      //send potato back to ringmaster
      printf("I'm it\n");
      status = send(master_socket_fd, &recv_potato, sizeof(recv_potato), 0);
      if(status < 0){
	perror("send potato back to ring master");
	return EXIT_FAILURE;
      }

      
    }
    else{
      //send potato to randomly chosen neighbor
      int random_neighbor = (rand()) % 2;
      int send_neighbor_fd;
      if(random_neighbor == 0){
	send_neighbor_fd = left_socket_fd;
      }
      else {
	send_neighbor_fd = right_socket_fd;
      }

      status = send(send_neighbor_fd, &recv_potato, sizeof(recv_potato), 0);
      if(status < 0){
	perror("send potato to random neighbor");
	return EXIT_FAILURE;
      }

    }
    
    
  }//while loop


  freeaddrinfo(host_info_list);
  freeaddrinfo(player_info_list);
  freeaddrinfo(right_info_list);
  close(right_socket_fd);
  close(left_socket_fd);
  close(master_socket_fd);
  close(player_socket_fd);
  return EXIT_SUCCESS;
}
