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

void Reaper(int sig){
    int status;
    
    while(waitpid(-1, &status, WNOHANG) >= 0);
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

    sock_fd = socket(AF_INET, sock_type, 0); //create socket
    if(sock_fd < 0) error_exit("failed socket(): %s\n", strerror(sock_fd));

    errno = bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr));
    if(errno < 0) error_exit("failed bind(): %s\n", service, strerror(errno));
    
    if(sock_type == SOCK_STREAM && listen(sock_fd, q_len) < 0){
        error_exit("failed listen(): %s\n", service, strerror(sock_type));
    }
 
    return sock_fd; 
}

void do_printenv(char *args[]){
    char *result = getenv(args[1]);
    //if(result == NULL) sprintf(result, "No Such env\n"); 
    //return result;
    printf("%s\n", result);
}

void do_setenv(char *args[]){
    setenv(args[1], args[2], 1);
}

void serve(int sockfd){
    char w_buffer[BUF_SIZE];
    char r_buffer[BUF_SIZE];

    //write welcome message to client
    sprintf(w_buffer, "****************************************\n");
    errno = write(sockfd, w_buffer, strlen(w_buffer));
    if(errno < 0) error_exit("failed write(): %s\n", strerror(errno));
    sprintf(w_buffer, "** Welcome to the information server. **\n");
    errno = write(sockfd, w_buffer, strlen(w_buffer));
    if(errno < 0) error_exit("failed write(): %s\n", strerror(errno));
    sprintf(w_buffer, "****************************************\n");
    errno = write(sockfd, w_buffer, strlen(w_buffer));
    if(errno < 0) error_exit("failed write(): %s\n", strerror(errno));
    sprintf(w_buffer, "%% ");
    errno = write(sockfd, w_buffer, strlen(w_buffer));
    if(errno < 0) error_exit("failed write(): %s\n", strerror(errno));
    
    while(1){
        for(int j = 0; j < BUF_SIZE; j++) w_buffer[j] = '\0';
        for(int j = 0; j < BUF_SIZE; j++) r_buffer[j] = '\0';   

        //read recv_msg from client
        errno = read(sockfd, r_buffer, BUF_SIZE);
        for(int j = 0; j < strlen(r_buffer); j++){
           if(r_buffer[j] == 13) r_buffer[j] = '\0';
        }
        if(errno < 0) error_exit("failed read(): %s\n", strerror(errno));
        /*else {
            printf("receive from client: %s\n", r_buffer);
            printf("message length: %d\n\n", strlen(r_buffer));
        }*/

        //parsing recv_msg
        char *args[100];
        int argn = 0;
        char *token_ptr;

        for(int j = 0; j < 100; j++) args[j] = NULL;

        token_ptr = strtok(r_buffer, " ");
        while(token_ptr != NULL){
            args[argn] = token_ptr;
            argn++;
            token_ptr = strtok(NULL, " ");
        }

        /*for(int j = 0; j < argn; j++){
            printf("token[%d]:%s\n", j,args[j]);
        }*/
        
        //executing commands
        char *result_str = NULL;
        if(strstr(r_buffer, "exit") != NULL){
            break;
        }

        int status, bytes_read;
        char buffer[BUF_SIZE];

        int pipe1fd[2]; //[1]child->[0]parent
        int pipe2fd[2]; //[0]
 
        if(pipe(pipe1fd) < 0 || pipe(pipe2fd) < 0){
            printf("failed pipe()\n"); 
            exit(-1);
        }

        if(strcmp(args[0], "setenv") == 0){ do_setenv(args);}
        pid_t pid = fork();
        if(pid < 0){ printf("failed fork()\n");  exit(-1); }
        else if(pid == 0){ //child
            close(pipe1fd[0]);
            dup2(pipe1fd[1], STDOUT_FILENO);
            if(strcmp(args[0], "printenv") == 0){ do_printenv(args); }
            else{
                errno = execvp(args[0], args);
                if(errno < 0 && strcmp(args[0],"setenv") != 0) printf("Unknown Command: [%s]\n", args[0]);
            }
            exit(0);
        }
        else{ //parent
            close(pipe1fd[1]);

            bytes_read = read(pipe1fd[0], buffer, BUF_SIZE);
            buffer[bytes_read] = '\0';
            //printf("%s\n", buffer);
            sprintf(w_buffer, buffer);
            errno = write(sockfd, w_buffer, strlen(w_buffer));

            waitpid(pid, &status, 0);
        }

        sprintf(w_buffer, "%% ");
        errno = write(sockfd, w_buffer, strlen(w_buffer));
        if(errno < 0) error_exit("failed write(): %s\n", strerror(errno));
    }
}

int main(int argc, char *argv[]){

    char *service; //int server_port;
    struct sockaddr_in client_addr;
    int master, slave; //master & slave sockfd
    unsigned int addrlen; //length of client's address

    if(argc != 2) error_exit("usage: %s port\n", argv[0]);

    service = argv[1];
    //1.create a master socket
    //2.bind to an address(IP:port)
    //3.place the master in passive mode for accepting connection requests
    master = passive_sock(service, "tcp", QUEUE_LEN);

    signal(SIGCHLD, Reaper); //prevent zombie process

    while(1){
        //4.repeatedly call accept() to receive connections and create slaves
        addrlen = sizeof(client_addr);
        slave = accept(master, (struct sockaddr *)&client_addr, &addrlen);
        if(slave < 0){ //-1 means failed accept()
            error_exit("failed accept(): %s\n", strerror(slave)); //strerror returns a pointer to string of error msgwith respect to input errer number
        }
        printf("Slave %d accept connection from %s:%d\n", slave, inet_ntoa(client_addr.sin_addr), (int)ntohs(client_addr.sin_port));

        setenv("PATH", "bin:.", 1);
        //5.create slave processes to handle connections and pass slave sockets to the processes
        int childpid;
        switch(childpid = fork()){ //return 0 to child, pid to parent
            case -1: error_exit("failed fork(): %s\n", strerror(errno));
            case 0: //child
                    chdir("/net/gcs/105/0556503/ras/");
                    printf("%s\n", getcwd(NULL, 0));
                    //6.interact with clients
                    close(master); //child doesn't need to use parent socket
                    serve(slave);
                    close(slave);
                    exit(0); //child processes terminate successfully
            default: //parent
                    close(slave); //parent doesn't need to use child socket
                    break;
        }

    }
}
