#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

//socket headers
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>  //include inet_addr

#define BUFFER_SIZE  1024
#define PORT 10001

int main(int argc, char **argv)
{
    char *host = "127.0.0.1";
    char buf[BUFFER_SIZE];
    int sockfd = -1;
    int ret = -1;
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("create socke error(%d): %s\r\n", errno, strerror(errno));
        return -1;
    }

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    //inet_pton(AF_INET, host, &server_addr.sin_addr.s_addr);
    server_addr.sin_addr.s_addr = inet_addr(host);
    server_addr.sin_port = htons(PORT);

    ret = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret == -1) {
        printf("connect error(%d): %s\r\n", errno, strerror(errno));
        close(sockfd);
        return -1;
    }
    
    int n = 0;
    while(1) {
        memset(buf, 0, sizeof(buf));
        if (fgets(buf, sizeof(buf) - 1, stdin) == NULL) {
            printf("fgets error(%d): %s\r\n", errno, strerror(errno));
            continue;
        }
        
        //n = write(sockfd, buf, strlen(buf));

        // 0:  send = wirte
        // n = send(sockfd, buf, strlen(buf), 0);
        
        //MSG_NOSIGNAL: ignore sigpipe signal. when  server_addr.sin_family is not set or sockefd
        // is not set, kernel will send sigpipe signal when userspace call send interface.
        // n = send(sockfd, buf, strlen(buf), MSG_NOSIGNAL);

        // MSG_DONTROUTE: in local network, not search route table
        n = send(sockfd, buf, strlen(buf), MSG_DONTROUTE);
        //only make sure copy datas from userspace to kernel network stack caches, not sure if datas be sent to server.
        if (n < 0) {
            printf("write error(%d): %s\r\n", errno, strerror(errno));
            continue;
        } else if (n == 0) {
            printf("write error(%d): %s\r\n", errno, strerror(errno));
            break;
        }

        memset(buf, 0, sizeof(buf));
        //n = read(sockfd, buf, sizeof(buf) - 1);
        n = recv(sockfd, buf, sizeof(buf) - 1, 0);
        if (n == 0) {
            printf("socket is closed");
            close(sockfd);
            return -1;
        } else if (n < 0) {
            printf("recv error(%d): %s\r\n", errno, strerror(errno));
            continue;
        } else {
            printf("recv data from server: %s\r\n", buf);
        }
    }

    close(sockfd);
    
    return 0;
}

