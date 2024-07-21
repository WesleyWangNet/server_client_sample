#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
//include poll
#include <poll.h>

#include <arpa/inet.h>


#define PORT 10001
#define BUFFER_SIZE  1024
#define MAX_CLIENTS  32


int main(int argc, char **argv)
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE] = {0};
    int ret = -1;
    struct pollfd fds[MAX_CLIENTS];
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("create socket error(%d): %s\r\n", errno, strerror(errno));
        return -1;
    }

    // allow reuse address and port
    int opt = 1;
    ret = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret != 0) {
        printf("set socket reuseaddr error(%d): %s\r\n", errno, strerror(errno));
        close(server_fd);
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    ret = bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0) {
        printf("bind error(%d): %s\r\n", errno, strerror(errno));
        close(server_fd);
        return -1;
    }

    ret = listen(server_fd, 10);
    if (ret < 0) {
        printf("listen error(%d): %s\r\n", errno, strerror(errno));
        close(server_fd);
        return -1;
    }

    //poll mode
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    int nfds = 1; // only one socket for server fd in initial state
    int poll_count = 0;
    socklen_t client_addr_len = 0;
    while(1) {
        client_fd = -1;
        poll_count = poll(fds, nfds, -1);
        if (poll_count < 0) {
            printf("poll error(%d): %s\r\n", errno, strerror(errno));
            break;
        }

        if (fds[0].revents & POLLIN) {
            memset(&client_addr, 0, sizeof(client_addr));
            client_addr_len = sizeof(client_addr);
            client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
            if (client_fd < 0) {
                printf("accept error(%d): %s\r\n", errno, strerror(errno));
                break;
            }

            printf("new connections from %s: %d\r\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            if (nfds == MAX_CLIENTS) {
                close(fds[client_fd].fd);
            } else {
                fds[nfds].fd = client_fd;
                fds[nfds].events = POLLIN;
                nfds++;
            }
        }

        int bytes_read = 0;
        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                memset(buffer, 0, sizeof(buffer));
                bytes_read = recv(fds[i].fd, buffer, BUFFER_SIZE - 1, 0);
                if (bytes_read <= 0) {
                    if (bytes_read == 0) {
                        memset(&client_addr, 0, sizeof(client_addr));
                        client_addr_len = sizeof(client_addr);
                        if (getpeername(fds[i].fd, (struct sockaddr *)&client_addr, &client_addr_len) == -1) {
                            printf("getpeername error(%d): %s\r\n", errno, strerror(errno));
                        } else {
                            char peer_addr[32] = {0};
                            inet_ntop(AF_INET, &client_addr.sin_addr, peer_addr, sizeof(peer_addr));
                            unsigned short peer_port = ntohs(client_addr.sin_port);
                            printf("%s:%d disconnection from peer\r\n", peer_addr, peer_port);
                        }
                    } else {
                        printf("recv error(%d): %s\r\n", errno, strerror(errno));
                    }
                    close(fds[i].fd);
                    fds[i] = fds[nfds-1];
                    nfds--;
                    i--; //check the last fd
                } else {
                    buffer[bytes_read] = '\0';
                    send(fds[i].fd, buffer, bytes_read, MSG_NOSIGNAL); //ignore sigpipe if client close socket
                }
            }
        }
    }

    if (server_fd > 0) {
        close(server_fd);
        server_fd = -1;
    }
    
    return 0;
}


