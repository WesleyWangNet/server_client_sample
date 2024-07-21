#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
//include poll
#include <sys/epoll.h>

//#include <sys/socket.h>
//#include <netinet/in.h>
#include <arpa/inet.h>



#ifdef LT_MODE
#define BUFFER_SIZE  1024
#else
#define BUFFER_SIZE  5
#endif
#define MAX_EVENTS 10
#define PORT 10001


static void set_nonblocking(int sockfd)
{
    int opts = fcntl(sockfd, F_GETFL);
    if (opts < 0) {
        perror("fcntl F_GETFL\r\n");
        return ;
    }

    opts = (opts | SOCK_NONBLOCK);
    if(fcntl(sockfd, F_SETFL, opts) < 0) {
        perror("fcntl F_SETFL\r\n");
    }
}

int main(int argc, char **argv)
{
    char buff[BUFFER_SIZE];
    int listen_sock, conn_sock, nfds;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = 0;
    
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1) {
        perror("create socket error\r\n");
        return -1;
    }

    // allow reuse address and port
    int opt = 1;
    if(setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
        printf("set socket reuseaddr error(%d): %s\r\n", errno, strerror(errno));
        close(listen_sock);
        return -1;
    }

    set_nonblocking(listen_sock);

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind \r\n");
        close(listen_sock);
        return -1;
    }

    if (listen(listen_sock, SOMAXCONN) == -1) {
        perror("listen\r\n");
        close(listen_sock);
        return -1;
    }

    //epoll mode
    struct epoll_event ev, events[MAX_EVENTS];
    int epollfd = -1;

    epollfd = epoll_create1(EPOLL_CLOEXEC);
    if (epollfd == -1) {
        perror("epoll_create1\r\n");
        close(listen_sock);
        return -1;
    }

    // LT mode, suitable for transmission with large amount of datas.
    // ET mode, suitable for transmission with small amount of datas.
#ifdef  LT_MODE
    ev.events = EPOLLIN;
#else   //ET_MODE
    ev.events = EPOLLIN | EPOLLET;
#endif
    ev.data.fd = listen_sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
        perror("epoll ctrl: listen sock\r\n");
        close(listen_sock);
        return -1;
    }

    while(1) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait\r\n");
            break;
        }

        int i = 0;
        for (i = 0; i < nfds; i++) {
            if (events[i].data.fd == listen_sock) {
                memset(&client_addr, 0, sizeof(client_addr));
                client_len = sizeof(client_addr);
                conn_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
                if (conn_sock == -1) {
                    perror("accepty\r\n");
                    continue;
                }
                printf("new connection: %s:%d\r\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                set_nonblocking(conn_sock);
                ev.events = EPOLLIN;
                ev.data.fd = conn_sock;

                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
                    perror("epoll ctl add \r\n");
                    close(conn_sock);
                    continue;
                }
            } else {
#ifdef LT_MODE
                int count = 0;
                memset(buff, 0, sizeof(buff));
                count = read(events[i].data.fd, buff, sizeof(buff) - 1);
                if (count <= 0) {
                    perror("read\r\n");
                    if (count == 0) {
                        memset(&client_addr, 0, sizeof(client_addr));
                        client_len = sizeof(client_addr);
                        if (getpeername(events[i].data.fd, (struct sockaddr *)&client_addr, &client_len) == -1) {
                            printf("getpeername error(%d): %s\r\n", errno, strerror(errno));
                        } else {
                            char peer_addr[32] = {0};
                            inet_ntop(AF_INET, &client_addr.sin_addr, peer_addr, sizeof(peer_addr));
                            unsigned short peer_port = ntohs(client_addr.sin_port);
                            printf("%s:%d disconnection from peer\r\n", peer_addr, peer_port);
                        }
                    }
                    close(events[i].data.fd);
                } else {
                    buff[count] = '\0';
                    write(events[i].data.fd, buff, count);
                }
#else // ET_MODE
                int count = 0;
                while(1) {
                    memset(buff, 0, sizeof(buff));
                    count = read(events[i].data.fd, buff, sizeof(buff));
                    if (count <= 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            printf("read finish\r\n");
                            break;
                        }
                        perror("read\r\n");
                        if (count == 0) {
                            memset(&client_addr, 0, sizeof(client_addr));
                            client_len = sizeof(client_addr);
                            if (getpeername(events[i].data.fd, (struct sockaddr *)&client_addr, &client_len) == -1) {
                                printf("getpeername error(%d): %s\r\n", errno, strerror(errno));
                            } else {
                                char peer_addr[32] = {0};
                                inet_ntop(AF_INET, &client_addr.sin_addr, peer_addr, sizeof(peer_addr));
                                unsigned short peer_port = ntohs(client_addr.sin_port);
                                printf("%s:%d disconnection from peer\r\n", peer_addr, peer_port);
                            }
                        }
                        close(events[i].data.fd);  // NOTICE: events[i].data.fd will be automatically removed from epoll when it's closed. there is no need to use epoll_ctl_del to delete it.
                        break;
                    } else {
                        buff[count] = '\0';
                        write(events[i].data.fd, buff, count);
                    }
                }
#endif
            }
        }
    }
         
       
    if (listen_sock > 0)
        close(listen_sock);

    if (epollfd > 0)
        close(epollfd);
        
    return 0;
}
