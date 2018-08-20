#include "player.h"
#include <stdio.h>
#include <assert.h>
#include <sstream>

#include "./pb/login.pb.h"
#include "../log.h"
#include "../kcpsess/sessServer.h"
#include "../conn.h"
#include "../qps.h"
#include "../net.h"


//#define POOL_PLAYEROBJ 4096
#define POOL_PLAYEROBJ 0

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

    playerMgr* playerMgr::m_inst = NULL;//new playerMgr;

    playerObj::playerObj(){
        LOG("new playerobj: %p", this);
    }
    playerObj::~playerObj(){
        LOG("del playerobj: %p", this);
    }
    void playerObj::init(int pid, unsigned long long rid, int roomid ){
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
        }
        m_sessid = _id; 
        KCPServer::m_sInst->BindConn(m_sessid);
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

    playerMgr::playerMgr(){
        initObjPool();
    }
    void playerMgr::AppendP(playerObj* p){
        m_ridPMap[p->getRid()] = p;
        m_pidPMap[p->getpid()] = p;
        LOG("playerMgr::AppendP pid: %d rid: %lld, roomid: %d p: %p", p->getpid(), p->getRid(), p->getRoomid(), p);
    }

    void playerMgr::AppendP(int pid, unsigned long long rid, char *acc , int roomid ){
        playerObj *p = popPlayer();//new playerObj(pid, rid, roomid );
        p->init(pid, rid, roomid);
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
        playerObj* p =  it->second;
        p->Offline();
        m_ridPMap.erase(it);
        m_pidPMap.erase(p->getpid());
        LOG("playerMgr::RemoveP pid: %d rid: %lld, roomid: %d p: %p", p->getpid(), p->getRid(), p->getRoomid(), p);
        pushPlayer(p);
    }

    void playerMgr::initObjPool(){
    if( netServer::g_netServer->isNet() ){
        return;
    }
        for(int i=0; i<POOL_PLAYEROBJ; i++){
            playerObj *p = new playerObj;
            m_pool.push(p);
        }
    }
    void playerMgr::pushPlayer(playerObj *r){
        m_pool.push(r);
    }
    playerObj* playerMgr::popPlayer(){
        playerObj *p = NULL;
        if(m_pool.empty()){
            p = new playerObj;
            LOG("!!!!!!!!!! unexpected error, pool playerObj* was recreated");
        }else{
            p = m_pool.front();
            m_pool.pop();
        }
        return p;
    }

    string playerMgr::DebugInfo(){
        stringstream ss;
        ss << "in kcpserver, m_ridPMap size:" << m_ridPMap.size() << ", playerobj pool size: " << m_pool.size() << endl;
        return ss.str();
    }


}
