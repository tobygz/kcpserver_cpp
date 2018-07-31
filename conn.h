#ifndef __conn_header__
#define __conn_header__

#include <queue>
#include "recvBuff.h"

using namespace std;


namespace net{

#define PACKET_SIZE 4
#define MSGID_SIZE 4
#define BUFFER_SIZE 32*1024
#define RECV_BUFFER_SIZE 4*1024
    class connObj{
        protected:
            int m_fd;
            int m_pid;
            
            bool m_bclose;

            int m_lastSec; //for timeout process
            char m_remoteAddr[64];

            char *m_psendBuf;
            unsigned m_sendBufCap;
            //char memchk[1024];
            unsigned int m_sendBufOffset;
            unsigned int m_sendBufLen; //amount
            //char memchkAA[1024];
            void chkmem();

            //recv buffer
            char m_NetBuffer[RECV_BUFFER_SIZE];
            int m_NetOffset;    //write offset
            int m_ReadOffset;    //read offset
            bool m_bChkReadZero;
            int _OnRead();
        public:
            connObj(int _fd);
            ~connObj(){
                delete[] m_psendBuf;
            }
            void SetPid(int _pid);
            int GetPid();
            int GetFd();
            int OnRead();
            void OnClose(bool btimeOut=false);
            void OnInit(char* paddr, bool);
            void resetBuffer();
            int send(unsigned char* buf, size_t size);

            virtual bool isGame(){return false;}
            virtual bool isNet(){return false; }
            virtual void dealMsg(msgObj *p) = 0;
            virtual bool parseBuff() = 0;
            virtual bool IsTimeout(int sec) = 0;

    };

    class connNetObj : public connObj{
        public:
            connNetObj(int _fd):connObj(_fd){}
            virtual void dealMsg(msgObj *p);
            virtual bool parseBuff();
            virtual bool IsTimeout(int sec);
            virtual bool isNet(){return true; }
    };

    //in game, game<--->net
    class connRpcObj : public connObj{
        protected:
            queue<sendCache*> m_sendCacheQueue;
            sendCache *m_pSendCache;

        public:
            queue<rpcObj*> m_queRpcObj;
            connRpcObj(int _fd):connObj(_fd){ m_pSendCache = NULL;}
            static connRpcObj* m_inst;
            virtual void dealMsg(msgObj *p){}
            virtual void dealRpcMsg(rpcObj*);
            virtual bool parseBuff();
            virtual bool IsTimeout(int sec){ return false;}
            virtual bool isGame(){return true;}
            void destroy();

            void rpcCallNet(char* target, unsigned long long pid, unsigned int msgid, unsigned char* pbyte, unsigned int byteLen);
            int dealSend();  
            void Update();       
    };
}
#endif
