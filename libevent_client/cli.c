#include <stdio.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>


#define PORT 10001

void read_cb(struct bufferevent *bev, void *arg)
{
    char buf[1024] = {0};

    bufferevent_read(bev, buf, sizeof(buf));
    printf("server send data: %s\r\n", buf);
}


void write_cb(struct bufferevent *bev, void *arg)
{
    printf("i am client, write data... done!\r\n");
}

void event_cb(struct bufferevent *bev, short events, void *arg)
{
    if (events & BEV_EVENT_EOF)
    {
        printf("connection closed\r\n");
    }
    else if (events & BEV_EVENT_ERROR)
    {
        printf("error\r\n");
    }
    else if (events & BEV_EVENT_CONNECTED)
    {
        printf("connected server\r\n");
        return ;
    }
    bufferevent_free(bev);
}

void read_terminal_cb(evutil_socket_t fd, short what, void *arg)
{
    char buf[1024] = {0};
    int len = read(fd, buf, sizeof(buf));
    struct bufferevent *bev = (struct bufferevent *)arg;
    bufferevent_write(bev, buf, len);
}


int main(int argc,char **argv)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct event_base *base = event_base_new();
    struct bufferevent *bev = bufferevent_socket_new(base,fd, BEV_OPT_CLOSE_ON_FREE);

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr.s_addr);

    int ret = bufferevent_socket_connect(bev, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    bufferevent_setcb(bev, read_cb, write_cb, event_cb, NULL);
    bufferevent_enable(bev, EV_READ);

    if(ret < 0) {
        perror("buifferevent socket connect error\r\n");
        return -1;
    }

    struct event *ev = event_new(base, STDIN_FILENO, EV_READ|EV_PERSIST, read_terminal_cb, bev);
    event_add(ev, NULL);
    event_base_dispatch(base);

    event_free(ev);
    bufferevent_free(bev);
    event_base_free(base);

    return 0;
}
