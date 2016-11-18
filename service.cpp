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

#define BUF_SIZE 30000
#define CMD_SIZE 2000
struct cmd{
    int type; //0:normal 1:|n 2:> xxx.txt 3:!n
    int argn;
    char *args[1800];
};

/*void do_printenv(char *args[]){
    char *result = getenv(args[1]);
    printf("%s\n", result);
}

void do_setenv(char *args[]){
    setenv(args[1], args[2], 1);
}*/
void send_welcome_msg(int sockfd, char *buffer);
int parse_received_msg(char *r_buffer, struct cmd* cmds);
int execute_cmds(int num_of_cmd, struct cmd* cmds, int sockfd);
void execute_single_cmd(struct cmd command, int i, int input_fd, int output_fd);
////////////////////////////////////////////////////////////////////////////
void serve(int sockfd){
    char w_buffer[BUF_SIZE], r_buffer[BUF_SIZE];
    int w_bytes, r_bytes;

    //Step1.send welcome messages
    send_welcome_msg(sockfd, w_buffer);

    sprintf(w_buffer, "%% ");
    w_bytes = write(sockfd, w_buffer, strlen(w_buffer));
    while(1){
        /*for(int j = 0; j < BUF_SIZE; j++){
            w_buffer[j] = '\0';
            r_buffer[j] = '\0';
        }*/

        //Step2.read messages from client
        bzero(r_buffer, BUF_SIZE);
        r_bytes = read(sockfd, r_buffer, BUF_SIZE);
        for(int j = 0; j < strlen(r_buffer); j++){
           if(r_buffer[j] == 13) r_buffer[j] = '\0';
        }

        if(strstr(r_buffer, "exit") != NULL){
            break;
        }

        //Step3.parse received messages
        struct cmd* cmds = (struct cmd*)malloc(sizeof(struct cmd)*30000);
        int num_of_cmd;
        num_of_cmd = parse_received_msg(r_buffer, cmds);

        printf("/////%d/////\n",num_of_cmd);
        for(int j = 0; j < num_of_cmd; j++){
            printf("cmd[%d] ", j+1);
            printf("type%d: ", cmds[j].type);
            for(int k = 0; k < cmds[j].argn; k++){
                printf("%s ", cmds[j].args[k]);
            }
            printf("\n");
        }
       
        int result_fd = execute_cmds(num_of_cmd, cmds, sockfd);
        bzero(w_buffer, BUF_SIZE);
        read(result_fd, w_buffer, BUF_SIZE);
        //w_buffer[strlen(w_buffer)] = '\n';
        write(sockfd, w_buffer, strlen(w_buffer));
 
        free(cmds);

        //Step4.

        bzero(w_buffer, BUF_SIZE);
        sprintf(w_buffer, "%% ");
        w_bytes = write(sockfd, w_buffer, strlen(w_buffer));
        if(w_bytes < 0) printf("failed write()\n");

    }
}

void send_welcome_msg(int sockfd, char *buffer){
    sprintf(buffer, "****************************************\n");
    write(sockfd, buffer, strlen(buffer));
    sprintf(buffer, "** Welcome to the information server. **\n");
    write(sockfd, buffer, strlen(buffer));
    sprintf(buffer, "****************************************\n");
    write(sockfd, buffer, strlen(buffer));
}

int parse_received_msg(char *r_buffer, struct cmd* cmds){
    char *token[30000];
    char *token_ptr;
    int num_of_token = 0;

    token_ptr = strtok(r_buffer, " ");
    while(token_ptr != NULL){
        token[num_of_token] = token_ptr;
        num_of_token++;
        token_ptr = strtok(NULL, " ");
    }
        
    int cmdPos[30000]={0};
    //for(int j = 0; j < 30000; j++) cmdPos[j] = 0;
    int num_of_cmd = 1;
        
    for(int j = 0; j < num_of_token; j++){
        //printf("token[%d]:%s\n", j,token[j]);
        if(token[j][0] == '|' || token[j][0] == '>' || token[j][0] == '!'){
            cmdPos[num_of_cmd] = j;
            num_of_cmd++;
        }
    }
    cmdPos[num_of_cmd] = num_of_token;
        
    for(int j = 0; j < num_of_cmd; j++){
        //printf("cmd[%d]: ", j+1);
        bool skip_pipe = false;
        cmds[j].argn = 0;
        if(token[cmdPos[j]][0] == '|' && strlen(token[cmdPos[j]]) == 1) skip_pipe = true;
        if(token[cmdPos[j]][0] == '|' && !skip_pipe){ cmds[j].type = 1; }
        else if(token[cmdPos[j]][0] == '!'){ cmds[j].type = 2; }
        else if(token[cmdPos[j]][0] == '>'){ cmds[j].type = 3; }
        else{ cmds[j].type = 0; }
        for(int k = cmdPos[j]; k < cmdPos[j+1]; k++){
            //printf("%s ", token[k]);
            if(skip_pipe){
                skip_pipe = false;
                continue;
            }
            cmds[j].args[cmds[j].argn] = token[k];
            cmds[j].argn++;    
        }
        //printf("\n");
        cmds[j].args[cmds[j].argn] = 0;
    }
    
    return num_of_cmd;
}

int execute_cmds(int num_of_cmd, struct cmd* cmds, int sockfd){
    int out_pipe[2];
    int result_pipe[2];

    if(pipe(out_pipe) == -1) printf("failed pipe()\n");
    if(pipe(result_pipe) == -1) printf("failed pipe()\n");

    //save original fd of stdin, stdout, stderr
    int stdin_fd = dup(0);
    int stdout_fd = dup(1);
    int stderr_fd = dup(2);

    int input_fd = 0; //read args of current cmd from here
    int output_fd = out_pipe[1]; //write the output of a command to this
    int next_input_fd = out_pipe[0]; //read next cmd's input from here

    for(int i = 0; i < num_of_cmd; i++){
        //change the value of file descriptor table 0,1
        //to the designated fd as new stdin and stdout
        dup2(input_fd, 0);
        dup2(output_fd, 1);

        execute_single_cmd(cmds[i], i,input_fd, output_fd);

        if(i != 0) close(input_fd); //can't close stdin
        close(output_fd);
    
        if(pipe(out_pipe) == -1) printf("failed pipe()\n");
        input_fd = next_input_fd;
        output_fd = out_pipe[1];
        next_input_fd = out_pipe[0];
    }

    //change the value back to original stdin and stdout
    dup2(stdin_fd, 0);
    dup2(stdout_fd, 1);

    return input_fd;    
}
void execute_single_cmd(struct cmd command, int i, int input_fd, int output_fd){
    char buffer[BUF_SIZE];
    bzero(buffer,BUF_SIZE);

    if(command.type == 0){
        if(strcmp(command.args[0], "printenv") == 0){
            char* result = getenv(command.args[1]);
            sprintf(buffer, "%s\n", result);
            write(output_fd, buffer, strlen(buffer));
        }
        else if(strcmp(command.args[0], "setenv") == 0){
            setenv(command.args[1], command.args[2], 1);
            write(output_fd, buffer, strlen(buffer));
        }
        else{
            pid_t pid= fork();
            int status;
            if(pid < 0){ exit(1); }
            else if(pid == 0){ //child
                execvp(command.args[0], command.args);
                char error_msg[100];
                sprintf(error_msg, "Unknown command:[%s]\n", command.args[0]);
                write(output_fd, error_msg, strlen(error_msg));
                exit(1);
            }
            else{ //parent
                waitpid(pid, &status, 0);
            }
        }
    }
    else if(command.type == 1){ //|n

    }
    else if(command.type == 2){ //!n

    }
    else{ //>
        FILE* fptr = fopen(command.args[1], "w");
        read(input_fd, buffer, BUF_SIZE);
        write(fileno(fptr), buffer, strlen(buffer));
    }
}
