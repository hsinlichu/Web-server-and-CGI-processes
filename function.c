/*b05505004朱信靂*/
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "function.h"

static void strdecode( char* to, char* from );
static int hexit( char c );
static void* e_malloc( size_t size );
static void* e_realloc( void* optr, size_t size );
static void add_to_buf( http_request *reqP, char* str, size_t len );
static char* get_request_line( http_request *reqP );

void init_request( http_request* reqP ) {
  reqP->conn_fd = -1;
  reqP->status = NO_USE;		// not used
  reqP->file[0] = (char) 0;
  reqP->query[0] = (char) 0;
  reqP->host[0] = (char) 0;
  reqP->buf = NULL;
  reqP->buf_size = 0;
  reqP->buf_len = 0;
  reqP->buf_idx = 0;
}

char* header(http_request* reqP,char* state,int length){
  char* output = (char *)malloc(10000 * sizeof(char));
  char buf[10000];
  time_t now;
  char timebuf[100];
  memset(output,'\0',sizeof(output));
  sprintf(buf,"HTTP/1.1 %s\015\012Server: SP TOY\015\012",state);
  strcat(output,buf);
  now = time( (time_t*) 0 );
  (void) strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
  sprintf(buf,"Date: %s\015\012",timebuf);
  
  if((strcmp("200 OK",state) == 0) && (strcmp(reqP->file,"info") != 0)){
    sprintf(map,"%s<br>CGI_Program: %s<br>Filename&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp: %s",buf,reqP->file,reqP->query);
    //printf("\nmap record: %s\n",map);
  }
  printf("| %s | %s | %s |\n",reqP->file,reqP->query,state);
  strcat(output,buf);
  sprintf(buf,"Content-Length: %d\015\012Content-Type: text/html\r\n\r\n",length);
  strcat(output,buf);
  return output;
}

char* fdmap(int fd,char* filename,int protection){
  char *output;
  struct stat fileInfo;
   if(fstat(fd, &fileInfo) == -1)
     ERR_EXIT("fstate error")
   if (fileInfo.st_size == 0)
     ERR_EXIT("file size is 0")
   output = mmap(NULL,fileInfo.st_size,protection, MAP_SHARED,fd, 0);
   if (output == MAP_FAILED)
     ERR_EXIT("Error mmapping the file")
   printf("success map \"%s\" size %lu\n",filename,fileInfo.st_size);
   return output;
 }
 
int checkFileName(http_request *reqP){
  int i = 0;
  int illegal = 0;
  char err;
  while((illegal == 0) && (reqP->file[i] != '\0')){
    if(!isalnum(reqP->file[i]))
      if(reqP->file[i] != '_'){
	err = reqP->file[i];
	illegal = 1;
      }
    ++i;
  }
  i = 0;
  while((illegal == 0) && (reqP->query[i] != '\0')){
    if(!isalnum(reqP->query[i]))
      if(reqP->query[i] != '_'){
	err = reqP->query[i];
	illegal = 2;
      }
    ++i;
  }
  int ret;
  struct stat sb;
  if(illegal == 0){
    ret = stat( reqP->file, &sb );
    if(ret < 0)
      illegal = 3;
  }
  if(illegal == 0){
    ret = stat( reqP->query, &sb );
    if(ret < 0)
      illegal = 4;
  }
  char desbuf[10000];
  char state[30];
  char title[10000];
  char detail[10000];
  char* headerbuf;
  memset(reqP->buf,'\0',reqP->buf_len);
  reqP->buf_len = 0;
  
  if(illegal != 0){
    if(illegal == 1){
      sprintf(title,"400 Bad Request");
      sprintf(detail,"Invalid CGI_program Name \"%s\"<br>Name Should not contain \"%c\"",reqP->file,err);
    }
    else if(illegal == 2){
      strcpy(state,"400 Bad Request");
      sprintf(title,"400 Bad Request");
      sprintf(detail,"Invalid Filename \"%s\"<br>Name Should not contain \"%c\"",reqP->query,err);
    }
    else if(illegal == 3){
      sprintf(title,"404 Not Found");
      sprintf(detail,"CGI_program Not Exist");
    }
    else if(illegal == 4){
      sprintf(title,"404 Not Found");
      sprintf(detail,"Query File Not Exist");
    }
    printf("illegal = %d\n%s\n%s\n",illegal,title,detail);
    sprintf(desbuf,HTMLformat,title,title,detail);
    headerbuf = header(reqP,title,strlen(desbuf));
    add_to_buf( reqP,headerbuf,strlen(headerbuf));
    add_to_buf( reqP, desbuf, strlen(desbuf));
    free(headerbuf);
    
    int nwritten = write(reqP->conn_fd,reqP->buf, strlen(reqP->buf));
    fsync(reqP->conn_fd);
    
#ifdef DEBUG
    fprintf(stdout,"======================write to browser======================\n");
    fprintf(stdout,"%s",reqP->buf);
    fprintf(stdout,"============================================================\n");
    fflush(stdout);
#endif

    return -1;
  }
  else{
    fprintf(stdout,"LEGAL FileName: %s\n",reqP->file);
    fprintf(stdout,"LEGAL QueryName: %s\n",reqP->query);
    return 0;
  }
}


int read_header_and_file( http_request* reqP, int *errP ) {
  // Request variables
  char* file = (char *) 0;
  char* path = (char *) 0;
  char* query = (char *) 0;
  char* protocol = (char *) 0;
  char* method_str = (char *) 0;
  int r, fd;
  struct stat sb;
  char timebuf[100];
  int buflen;
  char buf[10000];
  time_t now;
  void *ptr;

  // Read in request from client
  while (1) {
    r = read( reqP->conn_fd, buf, sizeof(buf) );
    //fprintf(stdout,"read ret %d\n",r);
    if ( r < 0 && ( errno == EINTR || errno == EAGAIN ) ) return 1;
    if ( r <= 0 ) ERR_RET( 1 );
    add_to_buf( reqP, buf, r );
    if ( strstr( reqP->buf, "\015\012\015\012" ) != (char*) 0 ||
	 strstr( reqP->buf, "\012\012" ) != (char*) 0 ) break;
  }
  // Parse the first line of the request.
  //printf("read from browser \n%s\n",buf);
  method_str = get_request_line( reqP );
  if ( method_str == (char*) 0 ) ERR_RET( 2 )
  path = strpbrk( method_str, " \t\012\015" );
  if ( path == (char*) 0 ) ERR_RET( 2 );
  *path++ = '\0';
  path += strspn( path, " \t\012\015" );
  protocol = strpbrk( path, " \t\012\015" );
  if ( protocol == (char*) 0 ) ERR_RET( 2 );
  *protocol++ = '\0';
  protocol += strspn( protocol, " \t\012\015" );
  query = strchr( path, '?' );
  if ( query == (char*) 0 )
    query = "";
  else
    *query++ = '\0';
  if ( strcasecmp( method_str, "GET" ) != 0 ) ERR_RET( 3 )
  else {
    strdecode( path, path );
    if ( path[0] != '/' ) ERR_RET( 4 )
    else file = &(path[1]);
  }

  if ( strlen( file ) >= MAXBUFSIZE-1 ) ERR_RET( 4 )
  if ( strlen( query ) >= MAXBUFSIZE-1 ) ERR_RET( 5 )
  strcpy( reqP->file, file );
  sscanf(query,"filename=%s",reqP->query);

  /* check info*/
  int pid;
  if(strcmp(reqP->file,"info") == 0){
    infoserver = reqP; 
    if((pid = fork()) < 0)
      ERR_EXIT("info fork error")
    else if(pid == 0){ // child process
      printf("info fork child\n");
      int ret;
      if((ret = raise(SIGUSR1)) < 0)
	ERR_EXIT("raise SIGUSR1 error")
      exit(0);
    }
    else{
      printf("fork info pid %d\n",pid);
      waitpid(pid,NULL,0);
      return 2;
    }
  }
  return 0;
}

int read_from_cgi_program( http_request* reqP, int *errP,char *map ) {
  struct stat sb;
  char timebuf[100];
  time_t now;
  int buflen;
  char buf[10000];
  char readbuffer[10000];
  int ret;
  char* headerbuf;
  char state[30];
  http_request tmp;
  init_request( &tmp );
  memset(reqP->buf,'\0',reqP->buf_len);
    reqP->buf_len = 0;
  while((ret = read(reqP->output[1][0],readbuffer,sizeof(readbuffer))) > 0){
    add_to_buf( &tmp, readbuffer, ret);
  }
  //printf("original from child process tmp buf\n%s",tmp.buf);
  int childStatus;
  waitpid(reqP->childpid,&childStatus,0);
  if(WEXITSTATUS(childStatus) == 1)
    strcpy(state,"404 Not Found");
  else if(WEXITSTATUS(childStatus) == 0)
    strcpy(state,"200 OK");
  headerbuf = header(reqP,state,tmp.buf_len);
  add_to_buf(reqP,headerbuf,strlen(headerbuf));
  add_to_buf(reqP, tmp.buf, tmp.buf_len); // dump the file
  free(headerbuf);

  return 0;
}

static char* get_request_line( http_request *reqP ) { 
  int begin;
  char c;
  char *bufP = reqP->buf;
  int buf_len = reqP->buf_len;

  for ( begin = reqP->buf_idx ; reqP->buf_idx < buf_len; ++reqP->buf_idx ) {
    c = bufP[ reqP->buf_idx ];
    if ( c == '\012' || c == '\015' ) {
      bufP[reqP->buf_idx] = '\0';
      ++reqP->buf_idx;
      if ( c == '\015' && reqP->buf_idx < buf_len && 
	   bufP[reqP->buf_idx] == '\012' ) {
	bufP[reqP->buf_idx] = '\0';
	++reqP->buf_idx;
      }
      return &(bufP[begin]);
    }
  }
  fprintf( stderr, "http request format error\n" );
  exit(1);
}

static void add_to_buf( http_request *reqP, char* str, size_t len ) { 
  char** bufP = &(reqP->buf);
  size_t* bufsizeP = &(reqP->buf_size);
  size_t* buflenP = &(reqP->buf_len);

  if ( *bufsizeP == 0 ) {
    *bufsizeP = len + 500;
    *buflenP = 0;
    *bufP = (char*) e_malloc( *bufsizeP );
  }
  else if ( *buflenP + len >= *bufsizeP ) {
    *bufsizeP = *buflenP + len + 500;
    *bufP = (char*) e_realloc( (void*) *bufP, *bufsizeP );
  }
  (void) memmove( &((*bufP)[*buflenP]), str, len );
  *buflenP += len;
  (*bufP)[*buflenP] = '\0';
}

void dumpinfo(http_request *reqP){
  printf("DumpInfo\n");
  char info[10000];
  char buf[10000];
  char* headerbuf;
  char content[10000];
  memset(reqP->buf,'\0',reqP->buf_len);
  reqP->buf_len = 0;

  /* info content*/
  sprintf(buf,"Number of processes died previously&nbsp:&nbsp<span style=\"color:#0088A8\"><b>%d</b></span><br>PIDs of Running Processes&nbsp:&nbsp<span style=\"color:#0088A8\"><b>",deadchild);
               
  strcpy(info,buf);
  int count = 0;
  for(int i = 4; i < maxfd; ++i)
    if(connectionSet[i])
      if(requestP[i].childpid != 0){
	sprintf(buf,"%d ",requestP[i].childpid);
	++count;
	strcat(info,buf);
      }
  if(count == 0)
    strcat(info,"None");
  if(strlen(map) == 0)
    sprintf(buf,"</b></span><br>Last Exit CGI&nbsp:<span style=\"color:#0088A8\"><b>None<b></span><br>");
  else
    sprintf(buf,"</b></span><br>Last Exit CGI&nbsp:<br><span style=\"color:#0088A8\"><b>%s<b></span><br>",map);
  strcat(info,buf);
  sprintf(content,HTMLformat,"Server Info","Server Info",info);
  /* content end */

  /* Header */
  headerbuf = header(reqP,"200 OK",strlen(content));
  add_to_buf(reqP,headerbuf,strlen(headerbuf));
  add_to_buf(reqP,content,strlen(content));
  free(headerbuf);

#ifdef DEBUG
  fprintf(stdout,"======================write to browser======================\n");
  fprintf(stdout,"%s",reqP->buf);
  fprintf(stdout,"============================================================\n");
  fflush(stdout);
#endif
  
  int nwritten;
  nwritten = write(reqP->conn_fd,reqP->buf, strlen(reqP->buf));
  fsync(reqP->conn_fd);
  return;
}


static void* e_malloc( size_t size ) {
  void* ptr;
  ptr = malloc( size );
  if ( ptr == (void*) 0 ) {
    (void) fprintf( stderr, "out of memory\n" );
    exit( 1 );
  }
  return ptr;
}

static void* e_realloc( void* optr, size_t size ) {
  void* ptr;
  ptr = realloc( optr, size );
  if ( ptr == (void*) 0 ) {
    (void) fprintf( stderr, "out of memory\n" );
    exit( 1 );
  }
  return ptr;
}

static void strdecode( char* to, char* from ) {
  for ( ; *from != '\0'; ++to, ++from ) {
    if ( from[0] == '%' && isxdigit( from[1] ) && isxdigit( from[2] ) ) {
      *to = hexit( from[1] ) * 16 + hexit( from[2] );
      from += 2;
    } else {
      *to = *from;
    }
  }
  *to = '\0';
}

static int hexit( char c ) {
  if ( c >= '0' && c <= '9' )
    return c - '0';
  if ( c >= 'a' && c <= 'f' )
    return c - 'a' + 10;
  if ( c >= 'A' && c <= 'F' )
    return c - 'A' + 10;
  return 0;           // shouldn't happen
}


