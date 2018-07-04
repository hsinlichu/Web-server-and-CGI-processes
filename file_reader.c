/*b05505004朱信靂*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h> 
#include <fcntl.h>

int main(){
  int ret;
  char buffer[10000];
  if((ret = read(STDIN_FILENO,buffer,sizeof(buffer))) < 0 )
     fprintf(stderr,"file reader read error\n");  
  char pathname[1024];
  sprintf(pathname,"./%s",buffer);
  int fd;
  if((fd = open(pathname,O_RDONLY)) < 0){
    fprintf(stderr,"FILE \"%s\" NOT EXIST\n",pathname);
    printf("<html> <head>\r\n");
    printf("<title>404 Not Found</title>\r\n");
    printf("</head>\r\n<body>\r\n");
    printf("<h1>404 Not Found</h1>\r\n");
    printf("<p>Unable to open data file,sorry!</p>\r\n");
    fprintf(stdout,"</body></html>\r\n");
    exit(1);
  }
  else{
    fprintf(stderr,"FILE \"%s\" OPEN SUCCESS\n",pathname);
    while((ret = read(fd,buffer,sizeof(buffer))) > 0)
      fprintf(stdout,"%s",buffer);
    close(fd);
  }
  //printf("Connection: close\015\012\015\012");
  fflush(stdout);
  exit(0);
}
