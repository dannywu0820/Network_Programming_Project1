#include"service.h"

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<netinet/in.h>

#include<time.h>
#include<sys/signal.h>
#include<sys/wait.h>
#include<sys/errno.h>
#include<sys/resource.h>
#include<stdarg.h>

#define QUEUE_LEN 32 //maximum connection queue length
#define BUF_SIZE 30000
////////////////////////////////////////////////////////////////////////
void Reaper(int sig){
    int status;
    
    while(waitpid(-1, &status, WNOHANG) >= 0); //waitpid should return immediately instead of waiting
}

int errno; //error number used for strerror()
unsigned short portbase = 0; //what's this?

int error_exit(const char *format, ...){
    va_list args; //list of arguments
    
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(-1);
}

int passive_sock(const char *service, const char *transport, int q_len){
    struct sockaddr_in addr; //an Internet endpoint address
    int sock_fd, sock_type;
    struct servent  *pse;   /* pointer to service information entry */
    struct protoent *ppe;   /* pointer to protocol information entry*/
    
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if(pse = getservbyname(service, transport))
        addr.sin_port = htons(ntohs((unsigned short)pse->s_port) + portbase);
    else if((addr.sin_port=htons((unsigned short)atoi(service))) == 0)
        error_exit("can't get \"%s\" service entry\n", service);
    
    if((ppe = getprotobyname(transport)) == 0)
        error_exit("can't get \"%s\" protocol entry\n", transport);

    if(strcmp(transport, "udp") == 0) sock_type = SOCK_DGRAM;
    else sock_type = SOCK_STREAM;

    //1.create a master socket
    sock_fd = socket(AF_INET, sock_type, 0); //create socket
    if(sock_fd < 0) error_exit("failed socket(): %s\n", strerror(sock_fd));

    //2.bind to an address(IP:port)
    errno = bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr));
    if(errno < 0) error_exit("failed bind(): %s\n", service, strerror(errno));
    
    //3.place the master in passive mode for accepting connection requests
    if(sock_type == SOCK_STREAM && listen(sock_fd, q_len) < 0){
        error_exit("failed listen(): %s\n", service, strerror(sock_type));
    }
 
    return sock_fd; 
}

int main(int argc, char *argv[]){

    char *service; //int server_port;
    struct sockaddr_in client_addr;
    int master, slave; //master & slave sockfd
    unsigned int addrlen; //length of client's address

    setenv("PATH", "bin:.", 1);
    chdir("/net/gcs/105/0556503/ras/");
    
    if(argc != 2) error_exit("usage: %s port\n", argv[0]);
    service = argv[1];
    
    master = passive_sock(service, "tcp", QUEUE_LEN);

    signal(SIGCHLD, Reaper); //prevent zombie process, still needs to figure out the principle

    while(1){
        //4.repeatedly call accept() to receive connections and create slaves
        addrlen = sizeof(client_addr);
        slave = accept(master, (struct sockaddr *)&client_addr, &addrlen);
        if(slave < 0){ //-1 means failed accept()
            error_exit("failed accept(): %s\n", strerror(slave)); //strerror returns a pointer to string of error msgwith respect to input errer number
        }
        printf("Slave %d accept connection from %s:%d\n", slave, inet_ntoa(client_addr.sin_addr), (int)ntohs(client_addr.sin_port));

        //5.create slave processes to handle connections and pass slave sockets to the processes
        int childpid;
        switch(childpid = fork()){ //return 0 to child, pid to parent
            case -1: error_exit("failed fork(): %s\n", strerror(errno));
            case 0: //child
                    printf("%s\n", getcwd(NULL, 0));
                    //6.interact with clients
                    serve(slave);
                    close(master); //child doesn't need to use parent sockfd
                    //serve(slave);
                    close(slave);
                    exit(0); //child processes terminate successfully
            default: //parent
                    close(slave); //parent doesn't need to use child sockfd
                    break;
        }

    }
}
