#include <fnmatch.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include <netinet/in.h>
#include <pthread.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define BACKLOG (10)

void serve_request(int);

struct thread_args
{
  int sockfd;
};

void *thread_func(void *args)
{
  struct thread_args *args_in = (struct thread_args *)args;
  serve_request(args_in->sockfd);
  /* Tell the OS to clean up the resources associated with that client
  * connection, now that we're done with it. */
  close(args_in->sockfd);
  pthread_exit(NULL);
}

char * error_str = "HTTP/1.0 404 Not Found\r\n"
                    "Content-Type: %s\r\n\r\n";

char *request_str = "HTTP/1.0 200 OK\r\n"
                    "Content-Type: %s\r\n\r\n";

char *index_hdr = "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\"><html>"
                  "<title>Directory listing for %s</title>"
                  "<body>"
                  "<h2>Directory listing for %s</h2><hr><ul>";

char QUERY_STRING[4095] = "";

// snprintf(output_buffer,4096,index_hdr,filename,filename);

char *index_body = "<li><a href=\"%s\">%s</a>";

char *index_ftr = "</ul><hr></body></html>";

/* char* parseRequest(char* request)
 * Args: HTTP request of the form "GET /path/to/resource HTTP/1.X" 
 *
 * Return: the resource requested "/path/to/resource"
 *         0 if the request is not a valid HTTP request 
 * 
 * Does not modify the given request string. 
 * The returned resource should be free'd by the caller function. 
 */
char *parseRequest(char *request)
{
  //assume file paths are no more than 256 bytes + 1 for null.
  char *buffer = malloc(sizeof(char) * 257);
  memset(buffer, 0, 257);

  if (fnmatch("GET * HTTP/1.*", request, 0))
    return 0;

  sscanf(request, "GET %s HTTP/1.", buffer);

  return buffer;
}

char * requested_file(char *path)
{
  const char * localPath = path;
  char * filename = malloc(4096);
  
  // int read_fd;
  // take requested_file, add a . to beginning, open that file
  filename[0] = '.';

  // TODO: parse the given path so that ONLY the filename exists.
  // Some examples of valid URLs that will not pass this skeleton code:
  // http://localhost:5000/ascii.txt?awesome=cs361
  // http://localhost:5000/doesnotexist <-- will cause program to crash
  // instead of giving a 404 response
  // More explicit TODO:
  // strip the entire resource path down to only the name of the file
  // based on the URL parsing rules to separate the **path** component
  // from the **query** component.

  // since we are only using localhost directories for this HW, everything after and including the third '/...' should be saved into filename[1]
  char * pch;
  pch = strchr(localPath, '?');
  int range = pch-localPath;
  char * parsedStr[1024];
  char finalString[4095] = "";

  if(pch != NULL && !strstr(localPath, "cgi-bin")){
    strncpy(parsedStr, localPath, range);
    strncpy(QUERY_STRING, localPath + range + 1, sizeof(localPath) + range + 10);
  }
  else{
    strcat(parsedStr, localPath);
  }
  
  strcat(finalString, parsedStr);
  
  strncpy(&filename[1], finalString, 4095);
  return filename;
}

int send_file(int read_fd, int send_fd)
{
  char mybuf[1024];
  size_t n;
  // Required fixes for this code:
  // Don't send **too many** bytes.
  // Read the entire file, even if it takes more than one call to read.
  // Send the entire file, even if it takes more than one call to write.
  // Close the file descriptors when you know you are done with them.
  
  while((n = read(read_fd, mybuf, sizeof(mybuf))) != 0){
    //printf("server recieved %ld bytes\n", n);
    if(n == -1)
      return 0;
    send(send_fd, mybuf, n, 0);
  }
  // you should only return 1 (success) if you successfully send the
  // entire file, otherwise return 0.
  return 1;
}

void serve_request(int client_fd)
{
  int read_fd;
  int file_offset = 0;
  char client_buf[4096];
  char send_buf[4096];
  char * filename;
  char * requested_resource;
  memset(client_buf, 0, 4096);
  while (1){
    file_offset += recv(client_fd, &client_buf[file_offset], 4096, 0);
    if (strstr(client_buf, "\r\n\r\n"))
      break;
  }

  requested_resource = parseRequest(client_buf);
  filename = requested_file(requested_resource);
  read_fd = open(filename, O_RDONLY);

  if( read_fd == -1 && (!strstr(filename, "cgi-bin"))){
    // no file in directory, commence error 404
    snprintf(send_buf, sizeof(send_buf), error_str, "text/html; charset=UTF-8");
    send(client_fd, send_buf, strlen(send_buf), 0);

    read_fd = open("./404.html", O_RDONLY);
    send_file(read_fd, client_fd);
    return;
  }
 
  // strip everything that isn't part of *the file name* off of the
  // resource request string, and add a "./" to the beginning to make
  // the path relative to CWD.
  
  //printf("FILENAME %s\n", filename);
  // is /cgi-bin/ the beginning of this request? then execute a
  // subprocess with posix spawn and pass the proper arguments via
  // environment variables.
  char *params;
  if (strstr(filename, "/cgi-bin/") && strtok(filename, "?")) {
    posix_spawn_file_actions_t actions;
    pid_t pid;
    char **newenv = malloc(sizeof(char *) * 2);
    char *env = malloc(1024);
    char ** argv = malloc(sizeof(char * ) * 2);
    argv[0] = filename;
    argv[1] = NULL;
    newenv[0] = env;
    newenv[1] = NULL;

    params = strtok(NULL, "");
    strcpy(env, "QUERY_STRING=");
    if (params != NULL) {
      strcat(env, params);
    }

    snprintf(send_buf, sizeof(send_buf), request_str, "text/plain; charset=UTF-8");
    send(client_fd, send_buf, strlen(send_buf), 0);

    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, client_fd, STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, client_fd);

    if(0 != posix_spawnp(&pid, argv[0], &actions, NULL, argv, newenv)){
      perror("spawn failed");
      exit(1);
    }
  } 
  else{
    if(strstr(filename, "html")){
      snprintf(send_buf, sizeof(send_buf), request_str, "text/html; charset=UTF-8");
    }
    else if(strstr(filename, "txt")){
      snprintf(send_buf, sizeof(send_buf), request_str, "text/plain; charset=UTF-8");
    }
    else if(strstr(filename, "png")){
      snprintf(send_buf, sizeof(send_buf), request_str, "image/png; charset=UTF-8");
    }
    else if(strstr(filename, "jpg")){
      snprintf(send_buf, sizeof(send_buf), request_str, "image/jpeg; charset=UTF-8");
    }
    else if(strstr(filename, "gif")){
      snprintf(send_buf, sizeof(send_buf), request_str, "image/gif; charset=UTF-8");
    }
    else if(strstr(filename, "pdf")){
      snprintf(send_buf, sizeof(send_buf), request_str, "application/pdf; charset=UTF-8");
    }
    
    send(client_fd, send_buf, strlen(send_buf), 0);
    read_fd = open(filename, O_RDONLY);
    send_file(read_fd, client_fd);    
  }

  return;
}

/* Your program should take two arguments:
 * 1) The port number on which to bind and listen for connections, and
 * 2) The directory out of which to serve files.
 */
int main(int argc, char **argv)
{
  /* For checking return values. */
  int retval;

  /* Read the port number from the first command line argument. */
  int port = atoi(argv[1]);

  // need to change directory to argv[2] so that
  // paths given in the URL = relative paths for your application.
  chdir(argv[2]);

  /* Create a socket to which clients will connect. */
  int server_sock = socket(AF_INET6, SOCK_STREAM, 0);
  if(server_sock < 0) {
    perror("Creating socket failed");
    exit(1);
  }

  /* A server socket is bound to a port, which it will listen on for incoming
     * connections.  By default, when a bound socket is closed, the OS waits a
     * couple of minutes before allowing the port to be re-used.  This is
     * inconvenient when you're developing an application, since it means that
     * you have to wait a minute or two after you run to try things again, so
     * we can disable the wait time by setting a socket option called
     * SO_REUSEADDR, which tells the OS that we want to be able to immediately
     * re-bind to that same port. See the socket(7) man page ("man 7 socket")
     * and setsockopt(2) pages for more details about socket options. */
  int reuse_true = 1;
  retval = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true, sizeof(reuse_true));
  if (retval < 0) {
    perror("Setting socket option failed");
    exit(1);
  }

  /* Create an address structure.  This is very similar to what we saw on the
     * client side, only this time, we're not telling the OS where to connect,
     * we're telling it to bind to a particular address and port to receive
     * incoming connections.  Like the client side, we must use htons() to put
     * the port number in network byte order.  When specifying the IP address,
     * we use a special constant, INADDR_ANY, which tells the OS to bind to all
     * of the system's addresses.  If your machine has multiple network
     * interfaces, and you only wanted to accept connections from one of them,
     * you could supply the address of the interface you wanted to use here. */
  struct sockaddr_in6 addr;   // internet socket address data structure                                                                              
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(port); // byte order is significant                                                                                         
  addr.sin6_addr = in6addr_any; // listen to all interfaces 
  retval = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));

  /* As its name implies, this system call asks the OS to bind the socket to
     * address and port specified above. */
  if (retval < 0) {
    perror("Error binding to port");
    exit(1);
  }

  /* Now that we've bound to an address and port, we tell the OS that we're
     * ready to start listening for client connections.  This effectively
     * activates the server socket.  BACKLOG (#defined above) tells the OS how
     * much space to reserve for incoming connections that have not yet been
     * accepted. */
  retval = listen(server_sock, BACKLOG);
  if (retval < 0) {
    perror("Error listening for connections");
    exit(1);
  }

  while (1)
  {
    /* Declare a socket for the client connection. */
    int sock;

    /* Another address structure.  This time, the system will automatically
         * fill it in, when we accept a connection, to tell us where the
         * connection came from. */
    struct sockaddr_in remote_addr;
    unsigned int socklen = sizeof(remote_addr);

    /* Accept the first waiting connection from the server socket and
         * populate the address information.  The result (sock) is a socket
         * descriptor for the conversation with the newly connected client.  If
         * there are no pending connections in the back log, this function will
         * block indefinitely while waiting for a client connection to be made.
         * */
    sock = accept(server_sock, (struct sockaddr*) &remote_addr, &socklen);
    if (sock < 0) {
      perror("Error accepting connection");
      exit(1);
    }

    /* At this point, you have a connected socket (named sock) that you can
         * use to send() and recv(). */

    /* ALWAYS check the return value of send().  Also, don't hardcode
         * values. */

    /* We create a thread pointer to pthread_t struct, and later pass it to 
         * pthread_create as an argument to store the ID of the new thread. 
         * We also create an args structure to store arguments for the routine */
    pthread_t thread;
    struct thread_args *args = (struct thread_args *)malloc(sizeof(struct thread_args));
    args->sockfd = sock;

    /* pthread_create starts a new thread and invokes thread_func (serve_request) 
    * with arguments (connected socket) */
    retval = pthread_create(&thread, NULL, &thread_func, (void *)args);
    if (retval)
    {
      printf("pthread_create() failed\n");
      exit(1);
    }
  }

  close(server_sock);
}
