#ifndef __net_header__
#define __net_header__

#include <queue>
#include <map>
#include <pthread.h>
#include <iostream>
#include <string>


using namespace std;
namespace net{

    enum NET_OP {
        NONE,
        NEW_CONN,
        DATA_IN,
        QUIT_CONN,
    };

    struct NET_OP_ST {
        NET_OP op;
        int fd;
        char paddr[64];
        bool bRpc;
    };

    int make_socket_non_blocking (int sfd);
    static int create_and_bind (char *port);
    unsigned int currentMs() ;

    class netServer{
        std::queue<NET_OP_ST*> m_netQueue;
        std::queue<NET_OP_ST*> m_netQueueRpc;
        std::map<int,bool> m_readFdMap;
        std::map<int,string> m_rpcFdMap; //rpc fd->name
        pthread_mutex_t *mutex ; 
        bool m_bNet;
        char m_name[32];
        public:
        static netServer *g_netServer; //for client
        static netServer *g_netRpcServer; //for other server rpc
        netServer();
        int m_sockfd;
        int m_epollfd;

        bool isNet(){ return m_bNet; }
        void setName(const char*);
        const char* getName(){ return (const char*) m_name; }
        int epAddFd(int fd, char* pname=NULL);
        int initSock(char *port);
        int initEpoll();
        void destroy();

        void appendSt(NET_OP_ST *pst, bool bmtx=true);
        void appendDataIn(int fd);
        void appendConnNew(int fd, char *pip, char* pport);
        void appendConnClose(int fd, bool bmtx=true);

        static bool g_run;
        static void* netThreadFun( void* );
        void queueProcessFun(); //for clients
        char* GetOpType(NET_OP );

        void queueProcessRpc(); //for rpc

    };

}
#endif