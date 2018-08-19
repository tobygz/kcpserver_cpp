#ifndef __HEADER_TCPCLIENT_BUFFER__
#define __HEADER_TCPCLIENT_BUFFER__

#include <map>
#include <queue>
#include <string>
#include <pthread.h>
#include "recvBuff.h"

#define RPC_BUFF_SIZE 4*1024*1024
using namespace std;
namespace net{

    class cautoLock {
        pthread_mutex_t *mutex ; 
        public:
        cautoLock( pthread_mutex_t *p){
            mutex = p;
            pthread_mutex_lock( p );
        }
        ~cautoLock(){
            pthread_mutex_unlock( mutex );
        }
    };

    class tcpclient;
    class tcpclientMgr {
        private:
            map<string, int> m_mapTcpClient; //name -> fd
            map<int, tcpclient*> m_mapFdTcpClient;
            tcpclientMgr(); 
            pthread_mutex_t *mutex ;  //for connmgr

            int processDealSend();
        public:
            tcpclient* createTcpclient(char* name, char* ip, int port);
            tcpclient* getTcpClient(const char* name);
            tcpclient* getTcpClientByFd(int fd);
            bool DelConn(int fd);
            void destroy();

            static void* readThread(void*);
            static void* writeThread(void*);

            void processAllRpcobj(unsigned int);

            static tcpclientMgr *m_sInst;

            void rpcCallGate(char* target, unsigned long long pid, unsigned int msgid, unsigned char* pbyte, unsigned int byteLen);
            void rpcCallGame(char* target, unsigned long long pid, unsigned int msgid, unsigned char* pbyte, unsigned int byteLen);            

    };

    class tcpclient {

        char m_name[128];
        char m_ip[128];
        int m_port;
        bool m_bNet;

        queue<rpcObj*> m_queRpcObj;
        queue<sendCache*> m_sendCacheQueue;
        sendCache *m_pSendCache;
        pthread_mutex_t *mutex ;  //for sendcache queue
        pthread_mutex_t *mutexRecv ;  //for recv queue

        char *m_pAddr;
        int m_sock;
        public:
        tcpclient(char* pname, char* ip, int port);

        char m_sName[512];

        char m_recvBuffer[RPC_BUFF_SIZE];
        int m_recvOffset;

        char* getName(){ return (char*) m_sName; }
        void update();

        int GetFd(){return m_sock;}
        int initSock();
        int doconnect();
        int OnRead();
        int dealReadBuffer();
        void handleRpcObj();

        int dealSend();
        void AppendSend(char* target, unsigned long long pid, unsigned int msgid, unsigned char* pbyte, unsigned int byteLen);
        //int dosend(sendCache*);

        void OnClose(bool force=false);
    };
}

#endif
