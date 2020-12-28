#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>

#define MAX_THREAD_NUMBER 10
#define UPLOAD 1
#define DOWNLOAD 2
#define SERVER_IP "192.168.255.129"
#define SERVER_PORT 1121

void* processMessage(void* args){
    int accept_fd;
    int option;
    char file_name[100];
    char recv_message[BUFSIZ];
    char *recv_message_str = recv_message;
    accept_fd = *((int*)args);
    printf("process: TID: %ld\n",syscall(SYS_gettid));
    int message_length = recv(accept_fd,recv_message,BUFSIZ, 0);
    if(message_length <= 0){
        return NULL;
    }
    recv_message[message_length] = '\0';
    //获取用户选项及文件名
    option = (int)recv_message[0];
    strcpy(file_name,recv_message + 2);
    //分类判定
    if(option == UPLOAD){
        //打开对应文件
        FILE *fd;
        fd = fopen(file_name,"w");
        if(fd == NULL){
            printf("no file!\n");
            return 0;
        }
        while(1){
            message_length = recv(accept_fd,recv_message,BUFSIZ, 0);
            if(message_length <= 0){
                break;
            }
            recv_message[message_length] = '\0';
            fputs(recv_message,fd);
        }
        printf("upload %s success!\n",file_name);
        fclose(fd);
    }
    else if(option == DOWNLOAD){
        FILE *fd;
        fd = fopen(file_name,"r");
        recv_message_str = recv_message;
        //读取文件
        while(fgets(recv_message_str,BUFSIZ,fd) != NULL){
            //发送数据
            int send_size = send(accept_fd, recv_message_str, strlen(recv_message_str), 0);
            //若实际发送大小不等于需要发送的大小
            while(send_size != strlen(recv_message_str)){
                recv_message_str = recv_message_str + send_size;
                send_size = send(accept_fd, recv_message_str, strlen(recv_message_str), 0);
            }
            recv_message_str = recv_message;
        }
        printf("download %s success!\n",file_name);
        fclose(fd);
    }
    close(accept_fd);
    return NULL;
}


int main() {
    int socket_fd,accept_fd[MAX_THREAD_NUMBER];
    pthread_t thread_ids[MAX_THREAD_NUMBER];
    int port = SERVER_PORT;
    struct sockaddr_in server_addr;
    int thread_index = 0;
    int accept_fd_index = 0;
    //如果出错则退出
    if( (socket_fd = socket(AF_INET,SOCK_STREAM,0)) < 0 ){
        printf("create socket error!\n");
        return -1;
    }
    //绑定端口及ip
    server_addr.sin_family = AF_INET;
    inet_aton(SERVER_IP,&server_addr.sin_addr);
    server_addr.sin_port = htons(port);
    while( bind(socket_fd,(struct sockaddr*)&server_addr,sizeof(server_addr)) == -1){
        printf("bind error!\n");
    }
    printf("bind success!\n");
    //监听
    while( listen(socket_fd, MAX_THREAD_NUMBER) == -1 ){
        printf("listen error!\n");
    }
    printf("Listening....\n");
    //接收
    while(1){
        if(accept_fd_index == MAX_THREAD_NUMBER + 1){
            accept_fd_index = 0;
        }
        if(thread_index == MAX_THREAD_NUMBER + 1){
            thread_index = 0;
        }
        accept_fd[accept_fd_index++] = accept(socket_fd, NULL, NULL);
        printf("Get the Client.\n");
        printf("main: TID: %ld\n",syscall(SYS_gettid));
        pthread_create(&thread_ids[thread_index++], NULL, processMessage, (void *)&accept_fd[accept_fd_index - 1]);
    }
}