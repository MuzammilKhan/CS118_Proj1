#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>   /* for the waitpid() system call */
#include <signal.h> /* signal name macros, and the kill() prototype */

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
  int sockfd, newsockfd, portno, pid;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;
  fd_set active_fd_set; // set for holding sockets

  if (argc < 2) {
      fprintf(stderr,"ERROR, no port provided\n");
      exit(1);
  }
  sockfd = socket(AF_INET, SOCK_STREAM, 0);  //create socket
  if (sockfd < 0) 
    error("ERROR opening socket");
  memset((char *) &serv_addr, 0, sizeof(serv_addr)); //reset memory
  //fill in address info
  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);
   
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    error("ERROR on binding");
     
  listen(sockfd,5);  //5 simultaneous connection at most
     
  /* Initialize the set of active sockets */
  FD_ZERO(&active_fd_set);
  /* put sock to the set, s.t. we can monitor whether a new connection request arrives */
  FD_SET(sockfd, &active_fd_set);
  while (1) {
    /* Block until some sockets are active. Let N is #sockets+1 in active_fd_set */
    if (select(sockfd + 1, &active_fd_set, NULL, NULL, NULL) < 0) {
      exit(-1); // error
    }
    /* Now some sockets are active */
    if (FD_ISSET(sockfd, &active_fd_set)) { // new connection request
      newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
      FD_SET(newsockfd, &active_fd_set);
    }
    /* Decide whether client has sent data */
    if (FD_ISSET(newsockfd, &active_fd_set)) {
      /* receive and process data here */
      int n;
      char buffer[256];
          
      memset(buffer, 0, 256);    //reset memory
      
      //read client's message
      n = read(newsockfd,buffer,255);
      if (n < 0) error("ERROR reading from socket");
      printf("Here is the message: %s\n",buffer);
     
      //reply to client
      n = write(newsockfd,"I got your message",18);
      if (n < 0) error("ERROR writing to socket");
    }
  } // end of while
   
  close(newsockfd);//close connection 
  close(sockfd);
  
  return 0; 
}
