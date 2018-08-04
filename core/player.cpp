#include "player.h"
#include <stdio.h>
#include <assert.h>
#include "./pb/login.pb.h"


#include "../log.h"
#include "../kcpsess/sessServer.h"
#include "../conn.h"



namespace net{
    void printBytes(unsigned char *val, size_t size, char *str) {
        const unsigned char* p = val;
        char info[4096] = "[";
        int i = 0;
        for (i = 0; i<size; i++) {
            if (i == 0) {
                sprintf(info, "%s%d", info, p[i]);
            }
            else {
                sprintf(info, "%s,%d", info, p[i]);
            }
        }
        strcat(info, "]");  
        sprintf(info,"%s len: %d", info, size );

        LOG("[%s] bin: %s\n", str, info);
    }

    playerMgr* playerMgr::m_inst = new playerMgr;

    playerObj::playerObj(int pid, unsigned long long rid, int roomid ){
        m_pid = pid;
        m_rid = rid;
        //strcpy(m_acc, acc);
        m_roomid = roomid;
    }

    void playerObj::sendComRetMsg(unsigned int msgid, int ret){
        S2CCommonRet_1000 *pmsg = new S2CCommonRet_1000;
        pmsg->set_ptid(msgid);
        pmsg->set_ret(ret);
        sendPbMsg(msgid, pmsg);
        delete pmsg;
    }
    void playerObj::sendMsg(unsigned char* pbuff, size_t size, int msgid){
        //call to net
        connRpcObj::m_inst->rpcCallNet((char*)"PushMsg2Client", m_pid, msgid, pbuff, size);
        LOG("playerMgr::sendMsg pid: %d rid: %ld, size: %d", m_pid, m_rid, size);
    }
    void playerObj::sendPbMsg(unsigned int msgid, ::google::protobuf::Message* pmsg){
        m_os.str("");
        pmsg->SerializeToOstream(&m_os);
        int len = m_os.str().size();
        assert(len+8 <1024);
        memcpy(m_sendBuf, &len, 4);
        memcpy(m_sendBuf+4, &msgid, 4);
        memcpy(m_sendBuf+8, (unsigned char*)m_os.str().c_str(), len );
        sendMsg((unsigned char*)m_sendBuf, len+8, msgid);
    }

    void playerObj::sendKcpMsg(unsigned char* pbuff, size_t size){
        LOG("playerObj::sendKcpMsg pid: %d rid: %d sessid: %d size: %d", m_pid, m_rid, m_sessid, size );
        printBytes(pbuff, size, (char*)"sendKcpMsg");
        //call kcp
        KCPServer::m_sInst->sendMsg(m_sessid, pbuff, size);
    }
    void playerObj::sendPbKcpMsg(unsigned int msgid, ::google::protobuf::Message* pmsg){
        pmsg->SerializeToOstream(&m_os);
        int len = m_os.str().size();
        assert(len+8 <1024);
        memcpy(m_sendBuf, &len, 4);
        memcpy(m_sendBuf+4, &msgid, 4);
        memcpy(m_sendBuf+8, (unsigned char*)m_os.str().c_str(), len );

        sendKcpMsg((unsigned char*)m_sendBuf, len+8);
    }

    void playerObj::Offline(){
        m_off = true;
        KCPServer::m_sInst->closeConn(m_sessid);
    }

    //for mgr
    //--------------------------------------------------------------------------------
    void playerMgr::AppendP(playerObj* p){
        m_ridPMap[p->getRid()] = p;
    }
    void playerMgr::AppendP(int pid, unsigned long long rid, char *acc , int roomid ){
        playerObj *p = new playerObj(pid, rid, roomid );
        m_ridPMap[rid] = p;
        LOG("playerMgr::AppendP pid: %d rid: %ld, acc: %s roomid: %d", pid, rid, acc ,roomid);
    }

    playerObj* playerMgr::GetP(int pid){
        for(map<unsigned long long,playerObj*>::iterator it = m_ridPMap.begin(); it!= m_ridPMap.end(); it++ ){
            if (it->second->getpid() == pid ){
                return it->second;
            }
        }
        return NULL;
    }
    playerObj* playerMgr::GetPByRid(unsigned long long rid){
        map<unsigned long long,playerObj*>::iterator it = m_ridPMap.find(rid);
        if( it == m_ridPMap.end()){
            return NULL;
        }
        return it->second;
    }

    void playerMgr::RemoveP(unsigned long long rid){
        map<unsigned long long,playerObj*>::iterator it = m_ridPMap.find(rid);
        if( it == m_ridPMap.end()){
            return ;
        }
        it->second->Offline();
        m_ridPMap.erase(it);
        delete it->second;
    }

}
