#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

//select header
#include <sys/select.h>

//socket headers
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>  //include inet_ntoa


#define BUFFER_SIZE  1024
#define PORT 10001
#define MAX_CLIENTS  32


#ifdef NONBLOCK_MODE
int main(int argc, char **argv)
{
    char buff[BUFFER_SIZE];
    int listenfd = -1;
    int clients_fd[MAX_CLIENTS] = {0};
    int ret = 1;
    
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        printf("create socket error(%d): %s\r\n", errno, strerror(errno));
        return -1;
    }

    // allow reuse address and port
    int opt = 1;
    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret != 0) {
        printf("set socket reuseaddr error(%d): %s\r\n", errno, strerror(errno));
        close(listenfd);
        return -1;
    }

    // set none block mode
    int flags = fcntl(listenfd, F_GETFL, 0);
    if (fcntl(listenfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        printf("fcntle socket non-block error(%d): %s\r\n", errno, strerror(errno));
        close(listenfd);
        return -1;
    }
    

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    ret = bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret == -1) {
        printf("bind socket error(%d): %s\r\n", errno, strerror(errno));
        close(listenfd);
        return -1;
    }

    //argc 2 is the number of storing unprocessed connections handles
    ret = listen(listenfd, 10);
    if (ret == -1) {
        printf("listen error(%d): %s\r\n", errno, strerror(errno));
        close(listenfd);
        return -1;
    }
    
    
    // ----------select----------
    fd_set readfds;
    int max_fd = 0;
    int stat = -1;
    int cli_fd = -1;
    int addrlen = 0;
    int cli_ret = -1;
    struct sockaddr_in client_addr;
    while (1) {
        max_fd = 0;
        stat = -1;
        cli_fd = -1;

        FD_ZERO(&readfds);
        FD_SET(listenfd, &readfds);
        
        max_fd = listenfd;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients_fd[i] > 0) {
                FD_SET(clients_fd[i], &readfds);
                if (clients_fd[i] > max_fd)
                    max_fd = clients_fd[i];
            }
        }

        stat = select(max_fd + 1, &readfds, NULL,NULL, NULL);
        if (stat < 0 && errno != EINTR) {
            printf("select error\r\n");
            break;
        }

        if (FD_ISSET(listenfd, &readfds)) {
            memset(&client_addr, 0, sizeof(client_addr));
            addrlen = sizeof(client_addr);
            if((cli_fd = accept(listenfd, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen)) < 0) {
                if (errno != EWOULDBLOCK && errno != EAGAIN)
                    printf("accept error(%d): %s\r\n", errno, strerror(errno));
                break;
            } else {
                printf("new connection: sockfd is: %d, ip is: %s, port is: %u\r\n", cli_fd, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                int index = 0;
                for (index = 0; index < MAX_CLIENTS; index++) {
                    if (clients_fd[index] == 0) {
                        clients_fd[index] = cli_fd;
                        break;
                    }
                }

                // the stored socket fds is exceeds the upper limits
                if (index == MAX_CLIENTS) {
                    close(cli_fd);
                    cli_fd = -1;
                }
            }
        }

        cli_fd = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            cli_fd = clients_fd[i];
            if (cli_fd <= 0)
                continue;

            if (FD_ISSET(cli_fd, &readfds)) {
                memset(buff, 0, sizeof(buff));
                cli_ret = read(cli_fd, buff, sizeof(buff) - 1);
                if (cli_ret == 0) {
                    memset(&client_addr, 0, sizeof(client_addr));
                    addrlen = sizeof(client_addr);
                    getpeername(cli_fd, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen);
                    printf("host [%s:%d] is disconnected !\r\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    close(cli_fd);
                    clients_fd[i] = 0;
                    continue;
                } else {
                    buff[cli_ret] = '\0';
                    send(cli_fd, buff, strlen(buff), 0);
                }
            }
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients_fd[i] > 0) {
            close(clients_fd[i]);
            clients_fd[i] = 0;
        }
    }

    close(listenfd);
    
    return 0;
}

#else  // block mode

int main(int argc, char **argv)
{
    char buff[BUFFER_SIZE];
    int listenfd = -1;
    int clients_fd[MAX_CLIENTS] = {0};
    int ret = 1;
    
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        printf("create socket error(%d): %s\r\n", errno, strerror(errno));
        return -1;
    }

    // allow reuse address and port
    int opt = 1;
    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret != 0) {
        printf("set socket reuseaddr error(%d): %s\r\n", errno, strerror(errno));
        close(listenfd);
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    ret = bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret == -1) {
        printf("bind socket error(%d): %s\r\n", errno, strerror(errno));
        close(listenfd);
        return -1;
    }

    //argc 2 is the number of storing unprocessed connections handles
    ret = listen(listenfd, 10);
    if (ret == -1) {
        printf("listen error(%d): %s\r\n", errno, strerror(errno));
        close(listenfd);
        return -1;
    }
    
    
    // ----------select----------
    fd_set readfds;
    int max_fd = 0;
    int stat = -1;
    int cli_fd = -1;
    int addrlen = 0;
    int cli_ret = -1;
    struct sockaddr_in client_addr;
    while (1) {
        max_fd = 0;
        stat = -1;
        cli_fd = -1;

        FD_ZERO(&readfds);
        FD_SET(listenfd, &readfds);
        
        max_fd = listenfd;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients_fd[i] > 0) {
                FD_SET(clients_fd[i], &readfds);
                if (clients_fd[i] > max_fd)
                    max_fd = clients_fd[i];
            }
        }

        stat = select(max_fd + 1, &readfds, NULL,NULL, NULL);
        if (stat < 0 && errno != EINTR) {
            printf("select error\r\n");
            break;
        }

        if (FD_ISSET(listenfd, &readfds)) {
            memset(&client_addr, 0, sizeof(client_addr));
            addrlen = sizeof(client_addr);
            if((cli_fd = accept(listenfd, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen)) < 0) {
                printf("accept error(%d): %s\r\n", errno, strerror(errno));
                break;
            }
            printf("new connection: sockfd is: %d, ip is: %s, port is: %u\r\n", cli_fd, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            
            int index = 0;
            for (index = 0; index < MAX_CLIENTS; index++) {
                if (clients_fd[index] == 0) {
                    clients_fd[index] = cli_fd;
                    break;
                }
            }
            
            // the stored socket fds is exceeds the upper limits
            if (index == MAX_CLIENTS) {
                close(cli_fd);
                cli_fd = -1;
            }
        }

        cli_fd = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            cli_fd = clients_fd[i];
            if (cli_fd <= 0)
                continue;

            if (FD_ISSET(cli_fd, &readfds)) {
                memset(buff, 0, sizeof(buff));
                cli_ret = read(cli_fd, buff, sizeof(buff) - 1);
                if (cli_ret == 0) {
                    memset(&client_addr, 0, sizeof(client_addr));
                    addrlen = sizeof(client_addr);
                    getpeername(cli_fd, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen);
                    printf("host [%s:%d] is disconnected !\r\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    close(cli_fd);
                    clients_fd[i] = 0;
                    continue;
                } else {
                    buff[cli_ret] = '\0';
                    send(cli_fd, buff, strlen(buff), 0);
                }
            }
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients_fd[i] > 0) {
            close(clients_fd[i]);
            clients_fd[i] = 0;
        }
    }

    close(listenfd);
    
    return 0;
}

#endif //non-block mode
