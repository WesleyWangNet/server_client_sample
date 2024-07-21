#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

//socket headers
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>   // include struct sockaddr_in, INADDR_ANY



#define BUFFER_SIZE  1024
#define SND_BUFFER_SIZE  2048
#define PORT 10001

int main(int argc, char **argv)
{
    char buff[BUFFER_SIZE];
    char snd_buf[SND_BUFFER_SIZE];
    int listenfd = -1;
    int connfd = -1;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    int addrlen;
    int ret = -1;
    
    
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        printf("create socket failed \r\n");
        return -1;
    }

    addrlen = sizeof(struct sockaddr_in);
    memset(&server_addr, 0, addrlen);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);
    ret = bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if(ret == -1) {
        printf("bind sock error: (errno: %d)%s\r\n", errno, strerror(errno));
        close(listenfd);
        return -1;
    }

    ret = listen(listenfd, 128);
    if (ret == -1) {
        printf("listen error(%d): %s\r\n", errno, strerror(errno));
        close(listenfd);
        return -1;
    }
    

    // block sock 
    connfd = accept(listenfd, (struct sockaddr *)&client_addr, &addrlen);
    if (connfd == -1) {
        printf("accept socket error(%d): %s\r\n", errno, strerror(errno));
        close(listenfd);
        return -1;
    }

    int count = 0;
    int len = 0;
    while(1) {
        memset(buff, 0, sizeof(buff));
        len = recv(connfd, buff, BUFFER_SIZE, 0);
        if (len > 0) {
            printf("server recv data: %s\r\n", buff);
        } else if (len == 0) {
            printf("clinet is offline\r\n");
            close(connfd);
            connfd = -1;
            break;
        } else {
            printf("recv data error(%d): %s\r\n", errno, strerror(errno));
            continue;
        }

        count++;
        memset(snd_buf, 0, sizeof(snd_buf));
        snprintf(snd_buf, sizeof(snd_buf) - 1, "echo message[%d]: %s", count, buff);
        len = -1;
        len = send(connfd, snd_buf, strlen(snd_buf), 0);
        if (len <= 0) {
            printf("send data error(%d): %s\r\n", errno, strerror(errno));
            continue;
        }
    }

    if (connfd > 0)
        close(connfd);
    close(listenfd);
    
    return 0;
}
