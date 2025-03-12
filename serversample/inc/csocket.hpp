#ifndef __CSOCKET_HPP__
#define __CSOCKET_HPP__

#include <sys/epoll.h>
#include <sys/socket.h>
#include <functional>
#include <string>
#include <string.h>
#include <vector>

#define _DATA_BUFSIZE_ 20  //大于包头
#define PKG_MAX_LENGTH     30000 
#define MAX_EVENTS 512 

#define PKG_HD_INIT 0
#define PKG_HD_RECVING 1
#define PKG_BD_INIT 2
#define PKG_BD_RECVING 3

using Connections_t = struct Connections_component;
using Connections_p = struct Connections_component *;
using listening_t = struct listening_component;
using listening_p = struct listening_component *;

class CSocket;
using event_handler_ptr = void (CSocket::*)(Connections_p);

struct listening_component {
    //用于初始化属性
    listening_component(): fd(-1), port(0), connections(nullptr) 
    {
    };
    int fd;
    int port;
    Connections_p connections;
};

struct Connections_component {
  //  using event_handler_ptr = void (*)(Connections_p conn);
     //给new 用于初始化属性
    Connections_component(): fd(-1), p_listen(nullptr), instance(0), iCurrsequence(0), w_ready(0), rhandler(nullptr), whandler(nullptr), curStat(0), precvbuf(nullptr), irecvlen(0), ifnewrecvMem(false), pnewMemPointer(nullptr), data(nullptr)
    {
        memset(dataHeadInfo, 0, _DATA_BUFSIZE_);
    };
    int fd;
    listening_p p_listen;
    
    unsigned char instance:1;
    uint64_t iCurrsequence;
    struct sockaddr s_sockaddr;

    uint8_t w_ready;
    
    event_handler_ptr rhandler;
    event_handler_ptr whandler;

    unsigned char curStat;
    char dataHeadInfo[_DATA_BUFSIZE_];
    char *precvbuf;
    unsigned int irecvlen;

    bool ifnewrecvMem;
    char *pnewMemPointer;
    Connections_p data;
};

class CSocket {
public:
    CSocket(int port, std::string ip_address);
    virtual ~CSocket();
    virtual bool Initialize();

    int epoll_init();
    int epoll_add_event(int fd, int readevent, int writeevent, uint32_t otherflag, uint32_t eventtype, Connections_p c);
    int epoll_process_events(int timer);

private:
    bool open_listening_sockets();
    bool close_listening_sockets();
    bool setnonblocking(int fd);

    void event_accept(Connections_p);
    void event_wait_request_handler(Connections_p);
    void close_connection(Connections_p);

    ssize_t recvproc(Connections_p conn, char *buff, ssize_t buflen);
    void wait_request_handler_proc_p1(Connections_p conn);
    void wait_request_handler_proc_plast(Connections_p conn);
    void tmpoutMsgRecvQueue(Connections_p);

    //size_t sock_ntop(struct sockaddr *sa, int port, u_char *text, size_t len);
    Connections_p get_connection(int isock);
    void free_connection(Connections_p conn);

private:
    int m_worker_connnections;
    int m_ListenPortCount;
    int m_epollhandle;

    Connections_p m_pConnections;
    Connections_p m_pfree_connections;

    int m_connection_n;
    int m_free_connection_n;

    //用来存储监听多个port的socket，本例只监听一个端口，存储一个
    //std::vector<std::unique_ptr<listening_p>> m_ListenSocketList;
    std::vector<listening_p> m_ListenSocketList;
    struct epoll_event m_events[MAX_EVENTS];

    size_t m_iLenPkgHeader;
    size_t m_iLenMsgHeader;

};

#endif