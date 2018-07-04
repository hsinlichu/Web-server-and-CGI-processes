/*b05505004朱信靂*/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <ctype.h>
#include "function.h"

char* logfilenameP;	// log file name
int maxfd;
char *map;
char *HTMLformat;
volatile int deadchild = 0;
http_request* requestP = NULL;  // pointer to http requests from client
http_request* infoserver = NULL;
int* connectionSet = NULL;

void catchSIGUR1(int signo);

void catchSIGINT(int signo);

static void init_http_server( http_server *svrP,unsigned short port);
// initailize a http_request instance, exit for error

static void free_request(http_request* reqP);
// free resources used by a http_request instance

static void set_ndelay( int fd );
// Set NDELAY mode on a socket.

int main( int argc, char** argv ) {
  http_server server;		// http server
  struct sockaddr_in cliaddr; // used by accept()
  int clilen;
  int conn_fd;		// fd for a new connection with client
  int err;			// used by read_header_and_file()
  int ret, nwritten;
  
  // Parse args. 
  if ( argc != 3 ) {
    (void) fprintf( stderr, "usage:  %s port# logfile\n", argv[0] );
    exit(1);
  }
  logfilenameP = argv[2];
  int logfd;
  if((logfd = open(logfilenameP,O_RDWR | O_CREAT,(mode_t)0600)) < 0)
     ERR_EXIT("open log file error")
  ftruncate(logfd,1000);
  map = fdmap(logfd,logfilenameP,PROT_READ | PROT_WRITE);
  close(logfd);
  int HTMLformatfd;
  if((HTMLformatfd = open("./HTMLformat",O_RDONLY)) < 0)
     ERR_EXIT("open log file error")
     HTMLformat = fdmap(HTMLformatfd,"./HTMLformat",PROT_READ);
  close(HTMLformatfd);
     
  if(signal(SIGUSR1, catchSIGUR1) == SIG_ERR)
    ERR_EXIT("signal error")
  if(signal(SIGINT, catchSIGINT) == SIG_ERR)
    ERR_EXIT("signal error")
      
  // Initialize http server
  init_http_server( &server, (unsigned short)atoi(argv[1]));

  maxfd = getdtablesize();
  requestP = ( http_request* ) malloc( sizeof(http_request) * maxfd );
  if (requestP == (http_request*)0) {
    fprintf( stderr, "out of memory allocating all http requests\n" );
    exit(1);
  }
  for (int i = 0; i < maxfd; i++ )
    init_request( &requestP[i] );
  requestP[ server.listen_fd ].conn_fd = server.listen_fd;
  requestP[ server.listen_fd ].status = READING;

  fprintf( stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d, logfile %s...\n",
	   server.hostname, server.port, server.listen_fd, maxfd, logfilenameP );
  
  struct timeval timeout;
  connectionSet = (int*)malloc(maxfd * sizeof(int));
  memset(connectionSet, 0, sizeof(connectionSet));

  // Main loop. 
  while (1) {
    // Wait for a connection.
    fd_set master_set;	
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;
    FD_ZERO(&master_set);
    for(int i = 0; i < maxfd; i++)
      if(connectionSet[i]) FD_SET(requestP[i].output[1][0], &master_set);
    FD_SET(3,&master_set);
    
    if(select(maxfd, &master_set, NULL, NULL, &timeout) < 0)
      ERR_EXIT("select error")
    if(FD_ISSET(3,&master_set)){         //if there is a new connection  
      clilen = sizeof(cliaddr);
      conn_fd = accept( server.listen_fd, (struct sockaddr *) &cliaddr, (socklen_t *) &clilen);
      // accept a connection on a socket.returns a new file descriptor referring to that socket. 
      if ( conn_fd < 0 ) {
	if ( errno == EINTR || errno == EAGAIN ) continue; // try again 
	if ( errno == ENFILE ) {
	  (void) fprintf( stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd );
	  continue;
	}	
	ERR_EXIT("accept")
      }
      requestP[conn_fd].conn_fd = conn_fd;
      connectionSet[conn_fd] = 1;
      requestP[conn_fd].status = READING;
      requestP[conn_fd].childpid = 0;
      strcpy( requestP[conn_fd].host, inet_ntoa( cliaddr.sin_addr ) );
      set_ndelay( conn_fd );
      fprintf( stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host );
      while(1){
	ret = read_header_and_file( &requestP[conn_fd], &err );
	if ( ret == 1) continue;
	else if(ret == 2){ // info
	  connectionSet[conn_fd] = 0;
	  close( requestP[conn_fd].conn_fd );
	  free_request( &requestP[conn_fd] );
	  break;
	}
	else if ( ret < 0 ) {
	// error for reading http header or requested file
	  fprintf(stderr,"error on fd %d, code %d\n",requestP[conn_fd].conn_fd, err);
	  requestP[conn_fd].status = ERROR;
	  connectionSet[conn_fd] = 0;
	  close( requestP[conn_fd].conn_fd );
	  free_request( &requestP[conn_fd] );
	  break;
	}
	else if ( ret == 0 ) {
	  // ready for writing
	  if(checkFileName(&requestP[conn_fd]) == -1){ // check file whether exist and the filename whether is valid
	    connectionSet[conn_fd] = 0;
	    close( requestP[conn_fd].conn_fd );
	    free_request( &requestP[conn_fd] );
	    ++deadchild;
	    fprintf(stdout,"===================Connection Closed===================\n\n");
	    break;
	  }
	  fprintf( stderr, "writing (buf %p, idx %d) %d bytes to request fd %d\n\n", 
		   requestP[conn_fd].buf, (int) requestP[conn_fd].buf_idx,
		   (int) requestP[conn_fd].buf_len, requestP[conn_fd].conn_fd );
	  fflush(stderr);
	  /* fork file to request query file*/
	  pid_t pid;
	  if(pipe(requestP[conn_fd].output[0]) < 0)
	    ERR_EXIT("pipe error\n")
	  if(pipe(requestP[conn_fd].output[1]) < 0)
	    ERR_EXIT("pipe error\n")
	  
	  if((pid = fork()) < 0)
	    ERR_EXIT("fork error")
	  else if(pid == 0){   // child process
	    dup2(requestP[conn_fd].output[0][0],STDIN_FILENO);
	    close(requestP[conn_fd].output[0][1]);
	    dup2(requestP[conn_fd].output[1][1],STDOUT_FILENO);
	    close(requestP[conn_fd].output[1][0]);
	    execl(requestP[conn_fd].file,requestP[conn_fd].file,(char*)0);
	  }
	  else{ // parent process
	    printf("fork pid %d\n",pid);
	    close(requestP[conn_fd].output[0][0]);
	    close(requestP[conn_fd].output[1][1]);
	    requestP[conn_fd].childpid = pid;
	    /* tell child the query filename */
	    int ret = write(requestP[conn_fd].output[0][1],requestP[conn_fd].query,strlen(requestP[conn_fd].query));
	    fsync(requestP[conn_fd].output[0][1]);
	    //printf("tell child the query filename ret %d\n",ret);
	    break;
	  }
	}
      }
    }
    
    int CompleteConnection = -1;
    for(int i = 0; i < maxfd; i++){  //find the ready file connection
      if(!connectionSet[i] || !FD_ISSET(requestP[i].output[1][0], &master_set)) continue;
      CompleteConnection = i;
      //fprintf(stdout,"%s response query=%s\n",requestP[CompleteConnection].file,requestP[CompleteConnection].query);
      break;
    }
    if(CompleteConnection == -1)continue;
    else{
      read_from_cgi_program(&requestP[CompleteConnection], &err,map);
      connectionSet[CompleteConnection] = 0;

#ifdef DEBUG
      fprintf(stdout,"======================write to browser======================\n");
      fprintf(stdout,"%s",requestP[CompleteConnection].buf);
      fprintf(stdout,"============================================================\n");
      fflush(stdout);
#endif
      nwritten = write(requestP[CompleteConnection].conn_fd, requestP[CompleteConnection].buf, strlen(requestP[CompleteConnection].buf));
      fsync(requestP[CompleteConnection].conn_fd);
      //fprintf( stderr, "complete writing %d bytes on fd %d\n", nwritten, requestP[CompleteConnection].conn_fd );
      close( requestP[CompleteConnection].conn_fd );
      free_request( &requestP[CompleteConnection] );
      
      close(requestP[conn_fd].output[0][1]);
      close(requestP[conn_fd].output[1][0]);
      ++deadchild;
      fprintf(stdout,"=====================Connection Closed======================\n\n");
    }
  }
  free( requestP );
  return 0;
}


// ======================================================================================================
// You don't need to know how the following codes are working

void catchSIGUR1(int signo){
  printf("CatchSIGUR1\n");
  dumpinfo(infoserver);
  infoserver = NULL;
  fprintf(stdout,"===================Connection Closed===================\n\n");
  
  return;
}
void catchSIGINT(int signo){
  unlink(logfilenameP);
  free(requestP);
  free(connectionSet);
  printf("\n***Server Termination***\n");
  exit(1);
}

static void free_request( http_request* reqP ) {
  if ( reqP->buf != NULL ) {
    free( reqP->buf );
    reqP->buf = NULL; 
  }
  init_request( reqP );
}

static void init_http_server( http_server *svrP, unsigned short port ) {
  struct sockaddr_in servaddr;
  int tmp;
  gethostname( svrP->hostname, sizeof( svrP->hostname) );
  svrP->port = port;
   
  svrP->listen_fd = socket( AF_INET, SOCK_STREAM, 0 );
  if ( svrP->listen_fd < 0 )
    ERR_EXIT( "socket" )
  bzero( &servaddr, sizeof(servaddr) );
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl( INADDR_ANY );
  servaddr.sin_port = htons( port );
  tmp = 1;
  if ( setsockopt( svrP->listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*) &tmp, sizeof(tmp) ) < 0 ) 
    ERR_EXIT ( "setsockopt " )
  if ( bind( svrP->listen_fd, (struct sockaddr *) &servaddr, sizeof(servaddr) ) < 0 )
    ERR_EXIT( "bind" )
  if ( listen( svrP->listen_fd, 1024 ) < 0 )
    ERR_EXIT( "listen" )
}

// Set NDELAY mode on a socket.
static void set_ndelay( int fd ) {
  int flags, newflags;
  flags = fcntl( fd, F_GETFL, 0 );
  if ( flags != -1 ) {
    newflags = flags | (int) O_NDELAY; // nonblocking mode
    if ( newflags != flags )
      (void) fcntl( fd, F_SETFL, newflags );
  }
}

