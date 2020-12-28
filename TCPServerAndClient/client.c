#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#define UPLOAD 1
#define DOWNLOAD 2
#define SERVER_IP "192.168.255.129"
#define SERVER_PORT 1121

int main() {
    int socket_fd;
    char send_message[BUFSIZ];
    int port = SERVER_PORT;
    char file_name[100];
    char file_content[BUFSIZ];
    char *file_content_str = file_content;
    struct sockaddr_in server_addr;
    int option;
    //如果出错则退出
    if( (socket_fd = socket(AF_INET,SOCK_STREAM,0)) < 0 ){
        printf("create socket error!\n");
        return -1;
    }
    //绑定端口及ip
    server_addr.sin_family = AF_INET;
    inet_aton(SERVER_IP,&server_addr.sin_addr);
    server_addr.sin_port = htons(port);
    //连接服务器
    while(connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == -1){
        printf("connect error!\n");
    }
    printf("connect success!\n");
    //用户选择需要上传还是下载
    printf("input 1 for upload,input 2 for download, please enter your option:\n");
    scanf("%d",&option);
    printf("please input the name of file\n");
    scanf("%s",file_name);
    //若为上传功能则发送上传请求
    if(option == UPLOAD){
        //要发送的信息第一个为选项
        send_message[0] = (char)option;
        send_message[1] = '#';
        //第二个为文件名
        strcpy(send_message + 2, file_name);
        send(socket_fd, send_message, strlen(send_message), 0);
        //打开对应文件
        FILE *fd;
        fd = fopen(file_name,"r");
        if(fd == NULL){
            printf("no file!\n");
            return 0;
        }
        file_content_str = file_content;
        //读取文件
        while(fgets(file_content_str,BUFSIZ,fd) != NULL){
            //发送数据
            int send_size = send(socket_fd, file_content_str, strlen(file_content_str), 0);
            //若实际发送大小不等于需要发送的大小
            while(send_size != strlen(file_content_str)){
                file_content_str = file_content_str + send_size;
                send_size = send(socket_fd, file_content_str, strlen(file_content_str), 0);
            }
            file_content_str = file_content;
        }
        fclose(fd);
    }
    else if(option == DOWNLOAD){
        //要发送的信息第一个为选项
        send_message[0] = (char)option;
        send_message[1] = '#';
        //第二个为文件名
        strcpy(send_message + 2, file_name);
        send(socket_fd, send_message, strlen(send_message), 0);
        //打开对应文件
        FILE *fd;
        fd = fopen(file_name,"w");
        while(1){
            char recv_message[BUFSIZ];
            int message_length = recv(socket_fd,recv_message,BUFSIZ, 0);
            if(message_length <= 0){
                break;
            }
            recv_message[message_length] = '\0';
            fputs(recv_message,fd);
        }
        fclose(fd);
    }
    //关闭socket
    close(socket_fd);
    return 0;
}