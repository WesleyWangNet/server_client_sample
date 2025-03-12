#include "csocket.hpp"
int main(int argc,char **argv) {
       CSocket *mysocket = new CSocket(10010, "127.0.0.1");
       mysocket->Initialize();
       mysocket->epoll_init();
       while(1) {
              mysocket->epoll_process_events(-1);        
       }
      


       delete mysocket;       
       return 0;
}