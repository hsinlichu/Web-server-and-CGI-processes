# Web-server-and-CGI-processes

## Usage
* How to compile the program: type "make" in terminal.
* How to run the server: ./server [port] [logname]
* How to access file via web browser: http://your_ip:port/cgi_program?filename=filename
* How to access server info via web browser: http://your_ip:port/info


## Functionality:

1. In server.c, I use select to do multi-tasking.

2. In server.c, when the server catch an interrupt signal, it would call signal handler catchSIGINT(int signo) to unlink log file and also free memory.

3. In function.c, fdmap() would map a file descriptor to memory, and i use it record info, header which write to conn_fd to accelerate file access from HTMLformat file.

4. In function.c, dumpinfo() would dump the info of the server with color.



## File detail:

1. server.c
2. file_reader.c
3. file_readerslow.c
4. function.h
5. function.c
6. README.txt
7. Makefile
8. HTMLformat  // contain HTML header
9. examplefile // for test


