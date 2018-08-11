#include "player.h"
#include <stdio.h>
#include <assert.h>
#include "./pb/login.pb.h"


#include "../log.h"
#include "../kcpsess/sessServer.h"
#include "../conn.h"
#include "../qps.h"



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
        m_sessid = 0;
        m_off = false;
        m_camp = 2;
        m_roomid = roomid;
    }

    void playerObj::setSessid(int _id){
        if(m_sessid != 0 ){
            KCPServer::m_sInst->rawCloseConn(m_sessid);
            //KCPServer::m_sInst->AppendInsteadFd(m_sessid);
        }
        m_sessid = _id; 
        m_off = false;
    }

    void playerObj::sendComRetMsg(unsigned int msgid, int ret){
        S2CCommonRet_1000 *pmsg = (S2CCommonRet_1000 *)&S2CCommonRet_1000::default_instance();
        pmsg->Clear();
        pmsg->set_ptid(msgid);
        pmsg->set_ret(ret);
        sendPbMsg(msgid, pmsg);
    }
    void playerObj::sendMsg(unsigned char* pbuff, size_t size, int msgid){
        //call to net
        connRpcObj::m_inst->rpcCallNet((char*)"PushMsg2Client", m_pid, msgid, pbuff, size);
    }
    void playerObj::sendPbMsg(unsigned int msgid, ::google::protobuf::Message* pmsg){
        m_os.str("");
        int bsize = pmsg->ByteSize();
        unsigned char* p = (unsigned char*) &bsize;
        m_os << *(p+0);
        m_os << *(p+1);
        m_os << *(p+2);
        m_os << *(p+3);
        p = (unsigned char*) &msgid;
        m_os << *(p+0);
        m_os << *(p+1);
        m_os << *(p+2);
        m_os << *(p+3);
        pmsg->SerializeToOstream(&m_os);
        sendMsg((unsigned char*)m_os.str().c_str(), bsize+8, msgid);
    }

    void playerObj::sendKcpMsg(unsigned char* pbuff, size_t size){
        //call kcp
        KCPServer::m_sInst->sendMsg(m_sessid, pbuff, size);
        qpsMgr::g_pQpsMgr->updateQps(3, size);
    }
    void playerObj::sendPbKcpMsg(unsigned int msgid, ::google::protobuf::Message* pmsg){
        if( m_sessid == 0 ){
            LOG("playerObj::sendPbKcpMsg pid: %d rid: %d sessid: %d failed for m_sessid is 0", m_pid, m_rid, m_sessid );
            return;
        }
        if( m_off ){
            return;
        }

        m_os.str("");
        int bsize = pmsg->ByteSize();
        unsigned char* p = (unsigned char*) &bsize;
        m_os << *(p+0);
        m_os << *(p+1);
        m_os << *(p+2);
        m_os << *(p+3);
        p = (unsigned char*) &msgid;
        m_os << *(p+0);
        m_os << *(p+1);
        m_os << *(p+2);
        m_os << *(p+3);
        pmsg->SerializeToOstream(&m_os);
        sendKcpMsg((unsigned char*)m_os.str().c_str(), bsize+8);
    }

    void playerObj::Offline(){
        m_off = true;
        KCPServer::m_sInst->closeConn(m_sessid);
    }

    //for mgr
    //--------------------------------------------------------------------------------
    void playerMgr::AppendP(playerObj* p){
        m_ridPMap[p->getRid()] = p;
        m_pidPMap[p->getpid()] = p;
        LOG("playerMgr::AppendP pid: %d rid: %lld, roomid: %d", p->getpid(), p->getRid(), p->getRoomid());
    }
    void playerMgr::AppendP(int pid, unsigned long long rid, char *acc , int roomid ){
        playerObj *p = new playerObj(pid, rid, roomid );
        AppendP(p);
    }

    playerObj* playerMgr::GetP(int pid){
        map<int,playerObj*>::iterator it = m_pidPMap.find(pid);
        if( it == m_pidPMap.end()){
            return NULL;
        }
        return it->second;
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
        m_pidPMap.erase(it->second->getpid());
        delete it->second;
    }

}
