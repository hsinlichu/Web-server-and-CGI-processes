/*b05505004朱信靂*/
#define TIMEOUT_SEC 3		// timeout in seconds for wait for a connection 
#define MAXBUFSIZE  1024	
#define NO_USE      0		// status of a http request
#define ERROR	    -1	
#define READING     1		
#define WRITING     2		
#define ERR_EXIT(a) { perror(a); exit(1); }

typedef struct {
  char hostname[512];		// hostname
  unsigned short port;	// port to listen
  int listen_fd;		// fd to wait for a new connection
} http_server;

typedef struct {
  int conn_fd;          	// fd to talk with client
  int status;			// not used, error, reading (from client)
                                // writing (to client)
  char file[MAXBUFSIZE];	// requested file:cgi_program
  char query[MAXBUFSIZE];	// requested query:requested filename
  char host[MAXBUFSIZE];	// client host
  char* buf;			// data sent by/to client
  size_t buf_len;		// bytes used by buf
  size_t buf_size; 		// bytes allocated for buf
  size_t buf_idx; 		// offset for reading and writing
  int output[2][2];             // use for openning pipe output[0] from server to cgi, output[1] from cgi to server
  int childpid;
} http_request;

void init_request(http_request* reqP);
// initailize a http_request instance

char* header(http_request* reqP,char* state,int length);

char* fdmap(int fd,char* filename,int protection);

int read_from_cgi_program( http_request* reqP, int *errP,char *map);

int checkFileName(http_request *reqP);

void dumpinfo(http_request *reqP);

int read_header_and_file( http_request* reqP, int *errP );
// return 0: success, file is buffered in retP->buf with retP->buf_len bytes
// return -1: error, check error code (*errP)
// return 1: continue to it until return -1 or 0
// error code: 
// 1: client connection error 
// 2: bad request, cannot parse request
// 3: method not implemented 
// 4: illegal filename// 5: illegal query
// 6: file not found
// 7: file is protected

#define ERR_RET( error ) { *errP = error; return -1; }
// return 0: success, file is buffered in retP->buf with retP->buf_len bytes
// return -1: error, check error code (*errP)
// return 1: read more, continue until return -1 or 0
// error code: 
// 1: client connection error 
// 2: bad request, cannot parse request
// 3: method not implemented 
// 4: illegal filename
// 5: illegal query
// 6: file not found
// 7: file is protected

extern char* logfilenameP;	// log file name
extern int maxfd;
extern char *map;
extern char *HTMLformat;
extern volatile int deadchild;
extern http_request* requestP;  // pointer to http requests from client
extern http_request* infoserver;
extern int* connectionSet;
