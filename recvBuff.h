#ifndef __HEADER_RECV_BUFFER__
#define __HEADER_RECV_BUFFER__

#define DEFAULT_RECV_SIZE 512
/*!
 * 内存由外部申请，
 * 释放在内部的析构函数中
 */

#define RPC_BUFF_SIZE 4*1024*1024
namespace net{
    enum eRecvType {
        eRecvType_lost = 0,
        eRecvType_connsucc,
        eRecvType_connfail,
        eRecvType_data,
    };

    class msgObj{
        unsigned long long m_uid;
        unsigned int *m_pmsgId;
        unsigned int *m_pbodyLen;
        unsigned char* m_pBody;        
        public:

        msgObj(unsigned int* msgid, unsigned int* plen, unsigned char* p);
        msgObj();
        ~msgObj();
        void init(unsigned int* msgid, unsigned int* plen, unsigned char* p);
        unsigned int getMsgid(){ return *m_pmsgId;}
        unsigned int getBodylen(){ return *m_pbodyLen;}
        unsigned char* getBodyPtr(){return m_pBody;}
        int size();
        void update();
    };

    class rpcObj{
            int *m_pLen;
            unsigned char *m_pbody;

            unsigned char m_pTarget[512]; //end with 0
            unsigned char m_pKey[512];    //end with 0
            unsigned char m_pParam[1024] ;  //end with 0
            unsigned char m_pResult[512] ; //end with 0

            unsigned char m_buff[256];

            unsigned char m_pMsgType;
            unsigned long long m_pPid;
            unsigned int m_pMsgid;
            unsigned char *m_pBuff; 
            unsigned int m_pBodyLen;

        public:
            rpcObj();
            void init();
            ~rpcObj();
            void decodeBuffer(char* p);
            static unsigned int encodeBuffer(unsigned char* p,char* target, 
                    unsigned long long pid, unsigned int msgid, unsigned char* pbyte, 
                    unsigned int byteLen);
            void update();
            void ToString();
            static bool chkPt(int val);
            static unsigned int getRpcSize(char* target, 
                    unsigned long long pid, unsigned int msgid, unsigned char* pbyte, 
                    unsigned int byteLen);

            unsigned char getMsgType(){return m_pMsgType;}
            unsigned char* getTarget(){ return m_pTarget;}
            unsigned char* getParam(){ return m_pParam;}
            unsigned char* getResult(){ return m_pResult;}
            unsigned long long getPid(){ return m_pPid;}
            unsigned int getMsgid(){ return m_pMsgid;}
            unsigned int getBodylen(){ return m_pBodyLen;}
            unsigned char* getBodyPtr(){ return m_pBuff;}
            
            
    };

    class sendCache {
        unsigned char m_mem[RPC_BUFF_SIZE];
        unsigned int m_offset;//write size
        unsigned int m_uid;
        static unsigned int g_guid;
        public:
        sendCache();
        void init();
        ~sendCache();
        unsigned char* getPtr(){ return (unsigned char*) m_mem; }
        unsigned int getOffset(){ return m_offset; }
        unsigned int getuid(){ return m_uid; }
        unsigned char* getWritePtr(){ return (unsigned char*)m_mem + m_offset; }
        unsigned int getLeftSize(){ return RPC_BUFF_SIZE-m_offset; }
        void updateOffset(unsigned int val){ m_offset += val ; }
        int dosend(int fd);
    };


}
#endif

