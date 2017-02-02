#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>   /* for the waitpid() system call */
#include <signal.h> /* signal name macros, and the kill() prototype */
#include <time.h> // for date time when return http response
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h> //to check if a file is a directory

void error(char *msg)
{
    perror(msg);
    exit(1);
}

// Generate http response for the http message and store it in response.
// Returns: 0 on success
//        -1 on error
int httpResponse(char* message, char* response, int response_buffer_size, int sock_fd) { 
  //TODO: Fix this function, it segfaults. 
  //we excpect only GET requests
  //need to consider case where client does localhost:port/  , here use href showing files accessible?
  //assuming we always use HTTP 1.1

  //parse message to determine encoding type of message body
  int content_type = -1; // use this to determine content type, -1:error 0:html 1:gif 2:jpeg
  int force404 = 0;
  int forceindex = 0;
  char filename[1024]; //assuming filename/path is no larger than 1024 bytes
  sscanf(message, "%*s %*c %s %*8s", filename);
  if(strstr(filename , ".html") != NULL){
    content_type = 0;
  } else if(strstr(filename , ".gif") != NULL){
    content_type = 1;
  } else if((strstr(filename , ".jpeg") != NULL) || (strstr(filename , ".jpg") != NULL)){
    content_type = 2;
  } else if(strstr(filename , ".ico") != NULL){
    content_type = 3;
  } else if(strstr(filename , "HTTP/1.1") != NULL){ //trying to access root directory
    strncpy(filename, ".\0", 2);
    content_type = 0;
    forceindex = 1;
  } else if(strstr(filename , ".") != NULL){ //unsupported filetype
    content_type = 0; //unkown content type
    force404 = 1;
  } else{ //possibly folder
    struct stat sb;
    if(stat(".", &sb) == 0 && S_ISDIR(sb.st_mode)){ //if we have access and it is a directory
      content_type = 0;
      forceindex = 1;
    } else {
      content_type = 0; //unkown content type
      force404 = 1;
    }

  }

  //determine header content
  int response_pos = 0;
  int fileExists = 1;
  //determine status
  FILE *fp;
  fp = fopen(filename, "r");
  if(fp == NULL || force404){ //file requested can't be found or error opening file
    strncpy(response + response_pos, "HTTP/1.1 404 Not Found\r\n", 24);
    response_pos = response_pos + 24;
    fileExists = 0;
    content_type = 0; //we will return an html page in this case
  } else { //everything ok
    strncpy(response + response_pos, "HTTP/1.1 200 OK\r\n", 17);
    
    response_pos = response_pos + 17;
  }

  //Date
  //shown how to do this at: http://stackoverflow.com/questions/7548759/generate-a-date-string-in-http-response-date-format-in-c
  char datetime[31];
  time_t now = time(0);
  struct tm tm = *gmtime(&now);
  strftime(datetime, sizeof(datetime), "%a, %d %b %b %Y %H:%M:%S %Z", &tm);
  datetime[29] = '\r';
  datetime[30] = '\n';
  

  strncpy(response + response_pos, datetime, 31);
  response_pos = response_pos + 31;  

  //Connection status
  strncpy(response + response_pos, "Connection: close\r\n", 19);
  response_pos = response_pos + 19;

  //Server field
  //do we need one of these? ex: Server: Apache/1.2.27

  //accept ranges
   //Todo: do we need another space?
  strncpy(response + response_pos, "Accept-Ranges: bytes\r\n", 22);
  response_pos = response_pos + 22;  

  //content type
  switch(content_type) {
    case 0:
      strncpy(response + response_pos, "Content-Type: text/html\r\n", 25);
      response_pos = response_pos + 25;  
      break;
    case 1:
      strncpy(response + response_pos, "Content-Type: gif\r\n", 19);
      response_pos = response_pos + 19;  
      break;
    case 2:
      strncpy(response + response_pos, "Content-Type: jpeg\r\n" , 20);
      response_pos = response_pos + 20;  
      break;
    case 3:
      strncpy(response + response_pos, "Content-Type: image/x-icon\r\n" , 28);
      response_pos = response_pos + 28;  
      break;
    default:
      break;
  }

  //Last modified field, do we need one?
  //ex: Last-Modified: Thu, 26 Jan 2017 21:32:00 GMT

  //empty line before message body TODO: check if necessary
    strncpy(response + response_pos, "\r\n", 2);
    response_pos = response_pos + 2; 
  if(fileExists && !forceindex) {
    //attach message body
    int filechar;
    while(1)
    {  //testing cases
      if(response_pos > response_buffer_size - 8) { //-8 to make sure there is space for ending cr & lfs
       //n = write(newsockfd, response_buffer, response_size);
        int n = write(sock_fd , response , response_pos);
        //sendto(sock_fd, response, response_pos, 0, dest_addr, dest_len);
        response_pos = 0;
      } 
      else {
            filechar = fgetc(fp);
            if( feof(fp) )
            {
             break ;
            }
            response[response_pos] = filechar;
            response_pos = response_pos + 1;
      }
    }  
    fclose(fp);
  }
  else if(force404){ //message for 404
    //response_pos = 0;
    //strncpy(response + response_pos, "HTTP/1.1 404 Not Found\r\n", 24);
    //response_pos = response_pos + 24;
    //      strncpy(response + response_pos, "Content-Type: text/html\r\n", 25);
    //response_pos = response_pos + 25;  
    strncpy(response + response_pos, "<html><body><h1>404 Not Found</h1></body></html>", 48);
    response_pos = response_pos + 48;
  }else { //message for index or forceindex

    DIR* dir;
    struct dirent *entry;
    dir = opendir(filename);
    if(dir != NULL){
      strncpy(response + response_pos, "<html><head>Files at this location: \n</head>\n" , 45);
      response_pos = response_pos + 45; 
      while ((entry = readdir (dir)) != NULL) {  
        if (entry->d_name[0] != '.' && entry->d_name[strlen(entry->d_name)-1] != '~'){// strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 ){
          if(response_pos > response_buffer_size - 40 - strlen(filename) - 2 * strlen(entry->d_name)) { // chunking and  make sure there is space for ending cr & lfs and other html
            int n = write(sock_fd , response , response_pos);
            response_pos = 0;
          } 
          //printf ("%s\n", entry->d_name);
          strncpy(response + response_pos, "<a href=\"" , 9);
          response_pos = response_pos + 9; 
          strncpy(response + response_pos, filename , strlen(filename));
          response_pos = response_pos + strlen(filename); 
          strncpy(response + response_pos, "/" , 1);
          response_pos = response_pos + 1;           
          strncpy(response + response_pos, entry->d_name, strlen(entry->d_name));
          response_pos = response_pos + strlen(entry->d_name); 
          strncpy(response + response_pos, "\">" , 2);
          response_pos = response_pos + 2; 
          strncpy(response + response_pos, entry->d_name, strlen(entry->d_name));
          response_pos = response_pos + strlen(entry->d_name);   
          strncpy(response + response_pos, "</a>\n" , 5);
          response_pos = response_pos + 5;     
      }
    }
    close(dir);
    strncpy(response + response_pos, "</head></html>" , 14);
          response_pos = response_pos + 14; 
    } else {
      //response_pos = 0;
      //strncpy(response + response_pos, "HTTP/1.1 404 Not Found\r\n", 24);
      //response_pos = response_pos + 24;
      //      strncpy(response + response_pos, "Content-Type: text/html\r\n", 25);
      //response_pos = response_pos + 25;  
      strncpy(response + response_pos, "<html><body><h1>404 Not Found</h1></body></html>", 48);
      response_pos = response_pos + 48;    
    }
  }

  strncpy(response + response_pos, "\r\n\r\n\0" , 5);
  response_pos = response_pos + 5; 
  return response_pos;
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
  /* Initialize the set of active sockets */
  //  FD_ZERO(&active_fd_set);
    /* put sock to the set, s.t. we can monitor whether a new connection request arrives */
  //  FD_SET(sockfd, &active_fd_set);


    /* Block until some sockets are active. Let N is #sockets+1 in active_fd_set */
    if (select(sockfd + 1, &active_fd_set, NULL, NULL, NULL) < 0) {
      close(newsockfd);//close connection
      close(sockfd);
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
      char buffer[512];
          
      memset(buffer, 0, 512);    //reset memory
      
      //read client's message
      n = read(newsockfd,buffer,511);
      if (n < 0){
        close(newsockfd);//close connection
        close(sockfd);
        error("ERROR reading from socket");
      }
      printf("Here is the message: %s\n",buffer);
     
      //Disable for part b? //TODO: double check this
      
      //reply to client
      //n = write(newsockfd,"Sending File",12);
      if (n < 0){
        close(newsockfd);//close connection
        close(sockfd);        
        error("ERROR writing to socket");
      } 
 
      int response_buffer_size = 2048; //TODO: Check how big this should be
      char* response_buffer = malloc(response_buffer_size);
      
      //memset(response_buffer,'p',2048);

      int response_size = httpResponse(buffer, response_buffer, response_buffer_size, newsockfd);
      if( response_size < 0) {
        free(response_buffer);
        close(newsockfd);//close connection
        close(sockfd);
        error("ERROR in httpResponse function");        
      }
      n = write(newsockfd, response_buffer, response_size);
      free(response_buffer);
      if(n < 0) {
      //if(write(newsockfd, "HTTP/1.1 200 OK\nDate: Mon, 27 Jul 2009 12:28:53 GMT\nServer: Apache/2.2.14 (Win32)\nLast-Modified: Wed, 22 Jul 2009 19:15:56 GMT\nContent-Length: 88\nContent-Type: text/html\nConnection: Closed\n<html>\n<body>\n<h1>Hello, World!</h1>\n</body>\n</html>\n\n")) {
        close(newsockfd);//close connection
        close(sockfd);
        error("ERROR writing to socket");
      }
      
    }
    close(newsockfd);//close connection  //TODO: CHECK IF THIS IS THE RIGHT LOCATION
  } // end of while(1)
   
  //we never reach here  
  return 0; 
}