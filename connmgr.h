#ifndef __conn_mgr_header__
#define __conn_mgr_header__

#include <map>
#include <queue>
#include <string>
#include "conn.h"

namespace net{
    class connObjMgr{
        map<int,connObj*> m_connMap; //sessid
        map<int,int> m_connFdMap; //fd->sessid
        map<int, string> m_connPidGameMap; // pid->game1
        map<int,int> m_writeFdMap; //fd->sessid


        //for gameserver
        map<string,int> m_nameFdMap; //net1->fd
        int maxSessid;
        connObj* rawGetConnByPid(int pid );
    public:
        connObjMgr();
        int GetOnline();
        void CreateConnBatch(void*,bool);
        void ChkConnTimeout();
        void AddWriteConn(int fd, connObj* p);

        void AppendNameConn(char* pname, connObj* p);

        connObj* GetConn(int fd);
        connObj* GetConnByPid(int pid );
        void DelConn(int pid, bool btimeOut=false );
        void destroy();
        void processAllWrite();
        static connObjMgr *g_pConnMgr ;

        void SendMsg(unsigned int pid, unsigned char* pmem, unsigned int size);
        void SendMsgAll(unsigned char* pmem, unsigned int size);
        void RegGamePid(unsigned int pid, char* pgame);
        string getGameByPid(int pid);
    };
}
#endif
