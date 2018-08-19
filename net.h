#ifndef __net_header__
#define __net_header__

#include <queue>
#include <map>
#include <pthread.h>
#include <iostream>
#include <string>
#include <sys/time.h>


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
    };

    int make_socket_non_blocking (int sfd);
    int create_and_bind (char *port);
    unsigned int currentMs() ;
    unsigned long long diffTime(timeval te, timeval ts);

    class netServer{
        std::queue<NET_OP_ST*> m_netQueue;
        std::map<int,bool> m_readFdMap;
        pthread_mutex_t *mutex ; 
        bool m_bNet;
        char m_name[32];

        //pool obj
        queue<NET_OP_ST *> m_opStPool;
        pthread_mutex_t *mutexSt ; 
        void initStPool();        
        NET_OP_ST* popSt();
        void pushSt(NET_OP_ST*);

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

        void rawAppendSt(NET_OP_ST *pst );
        void appendSt(NET_OP_ST *pst );
        void appendDataIn(int fd);
        void appendConnNew(int fd, char *pip, char* pport);
        void appendConnClose(int fd, bool bmtx=false);

        static bool g_run;
        static void* netThreadFun( void* );
        void queueProcessFun( unsigned int ); //for clients
        char* GetOpType(NET_OP );


    };

}
#endif
