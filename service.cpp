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
    int prev; //0:first 1:"|" 2:">"
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
////////////////////////////////////////////////////////////////////////////
void serve(int sockfd){
    char w_buffer[BUF_SIZE], r_buffer[BUF_SIZE];
    int w_bytes, r_bytes;

    //Step1.send welcome messages
    send_welcome_msg(sockfd, w_buffer);

    sprintf(w_buffer, "%% ");
    w_bytes = write(sockfd, w_buffer, strlen(w_buffer));
    while(1){
        for(int j = 0; j < BUF_SIZE; j++){
            w_buffer[j] = '\0';
            r_buffer[j] = '\0';
        }

        //Step2.read messages from client
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

        /*printf("/////%d/////\n",num_of_cmd);
        for(int j = 0; j < num_of_cmd; j++){
            printf("cmd[%d]: ", j+1);
            for(int k = 0; k < cmds[j].argn; k++){
                printf("%s ", cmds[j].args[k]);
            }
            printf("\n");
        }*/
        
        free(cmds);

        //Step4.

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
        cmds[j].argn = 0;
        for(int k = cmdPos[j]; k < cmdPos[j+1]; k++){
            //printf("%s ", token[k]);
            cmds[j].args[cmds[j].argn] = token[k];
            cmds[j].argn++;    
        }
        //printf("\n");
    }
    
    return num_of_cmd;
}
