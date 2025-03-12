
#include <iostream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <error.h>
#include <fcntl.h>
#include "csocket.hpp"
#include "commheader.hpp"
#include "common.hpp"

CSocket::CSocket(int port, std::string ip_address) 
{
    m_worker_connnections = MAX_CONNECTIONS;
    m_ListenPortCount = 1;

    m_epollhandle = -1;
    m_pConnections = NULL;
    
    m_pfree_connections = NULL;
    m_iLenPkgHeader = sizeof(COMM_PKG_HEADER);
    m_iLenMsgHeader = sizeof(MSG_HADER_STRUCT);
}

CSocket::~CSocket()
{
    // 非智能指针的释放
    for (auto pos: m_ListenSocketList) {
        delete pos;
    }
    m_ListenSocketList.clear();

    if(m_pConnections != NULL) {
        delete [] m_pConnections;
        m_pConnections = NULL;
    }

    //clear msgqueue
}

// 定义一个名为 CSoccket 的类中的 Initialize 成员函数
bool CSocket::Initialize()
{
    // 调用当前对象的 open_listening_sockets 成员函数
    // 如果该函数返回 false，表示打开监听套接字失败
    if(!open_listening_sockets()) {
        // 如果打开监听套接字失败，返回 false
        return false;
    }

    // 如果打开监听套接字成功，返回 true
    return true;
}

bool CSocket::setnonblocking(int fd)
{
    int opts;
    opts = fcntl(fd, F_GETFL);
    if(opts < 0) {
        return false;
    }
    opts = opts | O_NONBLOCK;
    if(fcntl(fd, F_SETFL, opts) < 0) {
        return false;
    }

    return true;
}


bool CSocket::open_listening_sockets()
{
    int isock;
    struct sockaddr_in serv_addr;
    int iport;
    char strinfo[100];

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //只绑定一个端口
    serv_addr.sin_port = htons(SERVERPORT);

    isock = socket(AF_INET, SOCK_STREAM, 0);
    if(isock == -1) {
        return false;
    }

    int opt = 1;
    if (setsockopt(isock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(isock);
        return false;
    }

    if(setnonblocking(isock) == false) {
        close(isock);
        return false;
    }

    if (bind(isock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        close(isock);
        return false;
    }

    //syn队列满后
    //开启tcp_abort_on_overflow后，队列满，server发送rst包
    //不开启，服务端不发任何包，直接丢弃，客户端会一直等待
    if (listen(isock, 128) == -1) {
        close(isock);
        return false;
    }
    
    //本例只会有一个监听端口
    auto p_listen = new listening_t;
    p_listen->fd = isock;
    p_listen->port = SERVERPORT;
    m_ListenSocketList.push_back(p_listen);
    
    return true;
}

bool CSocket::close_listening_sockets()
{
    for(auto pos:m_ListenSocketList) {
        close(pos->fd);
        delete pos;
    }
    m_ListenSocketList.clear();
    return true;
}

int CSocket::epoll_init(void)
{
    //epoll_create中的的m_worker_connections参数对epoll_create无效（操作系统不再使用该参数），只是用来表明连接池的大小
    m_epollhandle = epoll_create(m_worker_connnections);
    if(m_epollhandle == -1) {
        return -1;
    }
    m_connection_n = m_worker_connnections;
    m_pConnections = new Connections_t[m_worker_connnections];   //初始化为0 
    if(m_pConnections == NULL) {
        close(m_epollhandle);
        return -1;
    }

    //构建listening_component单链表
    int i = m_connection_n;
    Connections_p next = NULL;
    Connections_p c = m_pConnections;
    while(i > 0) {
        i--;
        c[i].fd = -1;
        c[i].instance = 1;
        c[i].p_listen = NULL;
        c[i].data = next;
        next = c;
    }
    m_free_connection_n = m_connection_n;
    //首次指向头节点m_pconnections[0]
    m_pfree_connections = next;

    //将监听套接字与conections_t绑定，本例中只有一个监听套接字
    //监听某个port对应--->Connections_t内存
    for(auto pos:m_ListenSocketList) {
        c = get_connection(pos->fd);
        if(c == NULL) {
            //没有可用资源
            exit(0);
        }
        c->p_listen = pos;
        pos->connections = c;
        
        //BOOK
        c->rhandler = &CSocket::event_accept;
        if(epoll_add_event( pos->fd, 
                            1,0,  //只关心读事件
                            0,   
                            EPOLL_CTL_ADD,
                            c) == -1) {
            exit(0);
        }
    }
    
    return 0;
}

int CSocket::epoll_add_event(int fd, int readevent, 
                             int writeevent, uint32_t otherflag, 
                             uint32_t eventtype, Connections_p c)
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));

    if (readevent == 1) {
        //三次握手连接进来属于一种可读事件
        //EPOLLRDHUP 客户端关闭连接，断连
        //默认LT水平触发，客户端连入，触发多次，直到accept处理或数据读完
        ev.events = EPOLLIN | EPOLLRDHUP;
    }

    if(otherflag != 0)
    {
        ev.events |= otherflag; 
    }

    //为判断事件是否过期。
    ev.data.ptr = (void *)((uintptr_t)c | c->instance); //将c和c->instance打包成一个指针

    if (epoll_ctl(m_epollhandle, eventtype, fd, &ev) == -1) {
        return -1;
    }
    return 0;
}


Connections_p CSocket::get_connection(int fd)
{
    Connections_p c = m_pfree_connections;
    if(c == NULL) {
        return NULL;
    }

    //处理空闲节点的偏移
    m_pfree_connections = c->data;
    m_free_connection_n--;

    //处理旧数据，（节点可能被使用过）序号和instance标识
    unsigned char instance = c->instance;
    uint64_t iCurrsequence = c->iCurrsequence;

    memset(c, 0, sizeof(Connections_t));
    c->fd = fd;
    c->curStat = PKG_HD_INIT;
    c->precvbuf = c->dataHeadInfo;
    c->irecvlen = sizeof(COMM_PKG_HEADER);

    //???
    c->ifnewrecvMem = false;
    c->pnewMemPointer = NULL;

    //用来检查是否是同一个连接，instann与epoll 中event.data.ptr做或运算，防止连接过期
    c->instance = !instance;
    //????? unused
   // c->iCurrsequence = iCurrsequence;
   // c->iCurrsequence++;
   
    return c;
}

void CSocket::event_accept(Connections_p c)
{
    struct sockaddr clisockaddr;
    socklen_t clilen = sizeof(clisockaddr);
    int err;

    do {
        // accept4(c->fd, &clisockaddr, &clilen， SOCK_NONBLOCK);
        int fd = accept(c->fd, &clisockaddr, &clilen);
        if (fd == -1) {
            err = errno;
            if(err == EINTR || err == ECONNABORTED) 
            {
                continue;
            } 
            else if (err == EAGAIN || err == EMFILE || err == ENFILE) //进程单个fd资源用尽
            {

            }
            return;
        }

        Connections_p newc = get_connection(fd);
        if(newc == NULL) {
            close(fd);
            return;
        }
        memcpy(&newc->s_sockaddr, &clisockaddr, clilen);
        if(setnonblocking(fd) == false) {
            free_connection(newc);
            return ;
        }
        newc->p_listen = c->p_listen;
        newc->w_ready = 1;
        newc->rhandler = &CSocket::event_wait_request_handler;

        if (epoll_add_event(fd, 1, 1, 0, EPOLL_CTL_ADD, newc) == -1) {
            free_connection(newc);
            return;
        }
        break;
    }while(1);
}

void CSocket::free_connection(Connections_p c)
{
    if (c->pnewMemPointer != NULL) {
        delete [] c->pnewMemPointer;
    }
    //插入链表头部
    c->data = m_pfree_connections;
    m_pfree_connections = c;
    m_free_connection_n++;    
    return;
}

void CSocket::close_connection(Connections_p pConn)
{
    if (pConn == NULL) {
        return;
    }
    close(pConn->fd);
    pConn->fd = -1;
    free_connection(pConn);
    return;
}

ssize_t CSocket::recvproc(Connections_p c, char *buff, ssize_t buflen)
{
    ssize_t n;
    n = recv(c->fd, buff, buflen, 0);
    if (n == 0) {
        //对端关闭socket
        close_connection(c);
        return n;
    }

    if(n < 0) {
        printf("recvproc: recv error: %d, %s\n", errno, strerror(errno));
        close_connection(c);
        return -1;
    }

    return n;
}

void CSocket::wait_request_handler_proc_p1(Connections_p c)
{
    LPCOMM_PKG_HEADER pPkgHeader = (LPCOMM_PKG_HEADER)c->dataHeadInfo;
    unsigned short pkglen = ntohs(pPkgHeader->pkgLen);
    if (pkglen > PKG_MAX_LENGTH || pkglen < m_iLenPkgHeader) {
        c->curStat = PKG_HD_INIT;
        c->precvbuf = c->dataHeadInfo;
        c->irecvlen = m_iLenPkgHeader;
        return ;
    } else {
        char *pTmpBuffer = new char[m_iLenMsgHeader + pkglen]{};
        if (pTmpBuffer == NULL) {
            return ;
        }
        c->pnewMemPointer = pTmpBuffer;
        LPMSG_HADER_STRUCT pTmpMsgHeader = (LPMSG_HADER_STRUCT)pTmpBuffer;
        pTmpMsgHeader->pConn = c;
       // pTmpMsgHeader->iCurrsequen = c->iCurrsequen;

       //重置包头，收取包体
       pTmpBuffer += m_iLenMsgHeader;
       memcpy(pTmpBuffer, c->dataHeadInfo, m_iLenPkgHeader);
       //处理空包体
        if(pkglen == m_iLenPkgHeader) {
            wait_request_handler_proc_plast(c);
        } else {
            c->curStat = PKG_BD_INIT;
            c->precvbuf = pTmpBuffer + m_iLenPkgHeader;
            c->irecvlen = pkglen - m_iLenPkgHeader;
       }
    }
    
    return;
}

void CSocket::event_wait_request_handler(Connections_p c)
{
    int n = recvproc(c, c->precvbuf, c->irecvlen);
    if (n <= 0) {
        return;
    }

    if (c->curStat == PKG_HD_INIT) 
    {
        if (n == m_iLenPkgHeader) {
            wait_request_handler_proc_p1(c);
        } else {
            c->curStat = PKG_HD_RECVING;
            c->precvbuf = c->precvbuf + n;
            c->irecvlen = c->irecvlen - n;
        }
    } 
    else if(c->curStat == PKG_HD_RECVING)
    {
        if (c->irecvlen == n) {
            wait_request_handler_proc_p1(c);
        } else {
            c->precvbuf = c->precvbuf + n;
            c->irecvlen = c->irecvlen - n;
        }
    } 
    else if (c->curStat == PKG_BD_INIT)
    {
        if(n == c->irecvlen) {
            wait_request_handler_proc_plast(c);
        } else {
            c->curStat = PKG_BD_RECVING;
            c->precvbuf = c->precvbuf + n;
            c->irecvlen = c->irecvlen - n;
        }
    } 
    else if (c->curStat == PKG_BD_RECVING)
    {
        if (n == c->irecvlen) {
            wait_request_handler_proc_plast(c);
        } else {
            c->precvbuf = c->precvbuf + n;
            c->irecvlen = c->irecvlen - n;
        }
    }
    return;
}

void CSocket::tmpoutMsgRecvQueue(Connections_p c)
{
    std::cout << "delete pkg buffer" << std::endl;
    if (c->pnewMemPointer != NULL) {
        delete []c->pnewMemPointer;
        c->pnewMemPointer = NULL;
    }
}

void CSocket::wait_request_handler_proc_plast(Connections_p c)
{
    //模拟业务处理，推入队列
    //取出连接做处理
    tmpoutMsgRecvQueue(c);

    //回收连接池
    c->curStat = PKG_HD_INIT;
    c->precvbuf = c->dataHeadInfo;
    c->irecvlen = m_iLenPkgHeader;
}
int CSocket::epoll_process_events(int timer)
{
    int events = epoll_wait(m_epollhandle, m_events, MAX_EVENTS, timer);
    if(events <= 0) {
        return -1;
    }
    Connections_p c;
    uintptr_t  instance;
    uint32_t i;

    for (int i = 0; i < events; i++) {
        c = (Connections_p)m_events[i].data.ptr;
        instance = (uintptr_t)c & 1;
        //取buffer真实地址
        c = (Connections_p)((uintptr_t)c & (uintptr_t)~1);
        if (c->fd == -1) {
            continue;
        }
        if(c->instance != instance) {
            //过期连接
            continue;
        }

        uint32_t revents = m_events[i].events;
        if(revents & (EPOLLERR | EPOLLHUP)) {
            revents |= EPOLLIN|EPOLLOUT;
        }
        if(revents & EPOLLIN) {
            (this->*(c->rhandler))(c);
        }
        if(revents & EPOLLOUT) {
            //
        }
    }
    return 0;
}
