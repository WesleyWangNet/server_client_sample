#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <netinet/in.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>




#define PORT 10001



void read_cb(struct bufferevent *bev, void *ctx)
{
    char buffer[256];
    int n;

#if 1
    struct evbuffer *input = bufferevent_get_input(bev);
    while((n = evbuffer_remove(input, buffer, sizeof(buffer))) > 0) {
        printf("received %.*s\n", n, buffer);
        bufferevent_write(bev, buffer, n);
    }
#else
    //Manipulate bufferevnets directly
    bufferevent_read(bev, buf, sizeof(buf));
    printf("client send data: %s\n", buf);
    char *response = "i am server, i got you.\n";
    bufferevent_write(bev, response, strlen(response));
#endif
}

void write_cb(struct bufferevent *bev, void *argc)
{
    printf("server wirte data done\r\n");
}

//event_cb is used to manage the handling of events such as successful connection, disconnection, error, etc.
void event_cb(struct bufferevent *bev, short events, void *ctx)
{
    if (events & BEV_EVENT_ERROR) {
        perror("ERROR from bufferevent\r\n");
    }
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        bufferevent_free(bev);
    }
    if (events & BEV_EVENT_CONNECTED) {
        struct sockaddr_in cli_addr;
        socklen_t client_len = sizeof(struct sockaddr_in);
        evutil_socket_t sock = (evutil_socket_t)(intptr_t)ctx;
        if (getpeername(sock, (struct sockaddr *)&cli_addr, &client_len) == -1) {
                perror("getpeername error");
        } else {
            char client_ip[16] = {0};
            inet_ntop(AF_INET, &cli_addr.sin_addr, client_ip, sizeof(client_ip));
            printf("%s: %u connected success\r\n", client_ip, ntohs(cli_addr.sin_port));
        }
    }

}

void listen_cb(struct evconnlistener *listener, evutil_socket_t sock, struct sockaddr *addr, int len, void *ptr)
{
    char ip[16] = {0};
    memset(ip, 0, sizeof(ip));
    //ptr corresponds to  the second argc in interface evconnlistener_new_bind
    struct sockaddr_in *cli_addr = (struct sockaddr_in *)&addr;

    //struct event_base *base = evconnlistener_get_base(listener);  or the following way to get base
    struct event_base *base = (struct event_base *)ptr;
    struct bufferevent *bev = bufferevent_socket_new(base, sock, BEV_OPT_CLOSE_ON_FREE);

    //bufferevent_setcb(bev, read_cb, write_cb, event_cb, NULL);
    bufferevent_setcb(bev, read_cb, write_cb, event_cb, (void *)sock);
    bufferevent_enable(bev, EV_READ | EV_WRITE);  //EV_WRITE can be unset because this property is enabled by default
    
}

void accept_error_cb(struct evconnlistener *listener, void *ctx)
{
    struct event_base *base = evconnlistener_get_base(listener);
    int err = EVUTIL_SOCKET_ERROR();
    fprintf(stderr, "Got an error %d(%s) on the listener, shutting down.\n",err, evutil_socket_error_to_string(err));
}

int main(int argc, char **argv)
{
    struct event_base *base = event_base_new();

    struct sockaddr_in svr_addr;

    memset(&svr_addr, 0, sizeof(struct sockaddr_in));
    svr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    svr_addr.sin_family = AF_INET;
    svr_addr.sin_port = htons(PORT);

    struct evconnlistener *evl = evconnlistener_new_bind(base,
                                                         listen_cb,
                                                         base,
                                                         LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
                                                         -1,
                                                         (struct sockaddr*)&svr_addr,
                                                         sizeof(svr_addr));

    evconnlistener_set_error_cb(evl, accept_error_cb);     

    event_base_dispatch(base);
    evconnlistener_free(evl);
    event_base_free(base);

    
    return 0;
}
