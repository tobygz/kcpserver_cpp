#include "room.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <sstream>


#include "../log.h"
#include "../recvBuff.h"
#include "../net.h"
#include "./pb/server.pb.h"
#include "../tcpclient.h"
#include "../qps.h"
#include "../kcpsess/sessServer.h"

#define FRAME_TICK_MS 66

//#define POOL_PT_2000_SIZE 1024*1024*16
//#define POOL_PT_2001_SIZE 1024*1024*16


#define POOL_PT_2000_SIZE 1024
#define POOL_PT_2001_SIZE 1024

//#define POOL_ROOMOBJ 1024
#define POOL_ROOMOBJ 0

using namespace pb;

namespace net{

    roomMgr* roomMgr::m_inst = NULL; //new roomMgr;

    void roomObj::init(int roomid){
        m_roomid = roomid;
        m_frameTick = FRAME_TICK_MS;
        m_frameId = 0;
        m_endFrameId = 2147483648;
        m_gateOver = false;
        m_brun = false;
        m_bornMs = 0;
        m_startMs = 0;
        m_accMs = 0;
        m_lastMs = 0;

        m_tmpDiff = 0;

        memset(m_buf,0,128);

        m_pt2001 = NULL;

        m_allOperMap.clear();
        m_ridPMap.clear();
        m_pidPMap.clear();
        m_pidConn.clear();
    }

    roomObj::roomObj(){
        LOG("new roomobj %p", this);
    }
    roomObj::~roomObj(){
        LOG("del roomobj %p", this);
    }

    void roomObj::UpdateDelta(unsigned int delta){
        m_frameTick = (delta&0xffff0000) >> 16;
        LOG("UpdateDelta roomid: %d val: %d", m_roomid, m_frameTick );
    }

    void roomObj::EnterP(playerObj* p, void* pconn){
        UDPConn *pu = (UDPConn*)pconn;
        m_ridPMap[p->getRid()] = p;
        m_pidPMap[p->getpid()] = p;
        if(pu){
            m_pidConn[p->getpid()] = (void*)pu;
        }
        SvrFrameCmd(p, 1);
        LOG("EnterP roomid: %d rid: %d camp: %d sessid: %d", m_roomid, p->getRid(), p->getCamp(), p->getSessid() );
    }

    void roomObj::LeaveP(playerObj* p){
        SvrFrameCmd(p, 0);
        LOG("LeaveP roomid: %d rid: %d camp: %d", m_roomid, p->getRid(), p->getCamp() );
    }

    int roomObj::Update(unsigned int ms){
        if(m_gateOver){
            RawOver();
            return 1;
        }
        if(!m_brun){
            return 0;
        }

        if(m_ridPMap.size()==0){
            return 0;
        }

        if( m_lastMs == 0 ){
            m_lastMs = ms;
            m_tmpDiff = ms;
            m_accMs = 0;
            return 0;
        }

        m_accMs += ms - m_lastMs;
        m_lastMs = ms;

        while(m_accMs>=m_frameTick){
            m_accMs -= m_frameTick;
            if(ms-m_tmpDiff > m_frameTick+10){
                LOG("roomobj::update roomid: %d ms: %u m_frameId: %d m_accMs: %u m_lastMs: %u diff: %d",m_roomid, ms, m_frameId, m_accMs, m_lastMs, ms-m_tmpDiff );
            }
            if(!m_pt2001){
                m_pt2001 = roomMgr::m_inst->fetchPt2001();
            }
            m_pt2001->set_frameid(m_frameId);
            m_tmpDiff = ms;
            m_allOperMap[m_frameId] = m_pt2001;
            //broadcast msg
            BroadcastKcp(2001, m_pt2001);
            m_pt2001 = roomMgr::m_inst->fetchPt2001();
            m_frameId++;
        }

        if(m_frameId>=m_endFrameId && m_endFrameId != 2147483648){
            RawOver();
            return 1;
        }
        return 0;
    }


    void roomObj::GetFramesData(playerObj* p, char* cobj ){
        msgObj* obj = (msgObj*) cobj;
        C2SGetRepairFrameData_2002 *pmsg = (C2SGetRepairFrameData_2002*)&C2SGetRepairFrameData_2002::default_instance();
        pmsg->Clear();
        string val((char*)obj->getBodyPtr(), obj->getBodylen());
        istringstream is(val);
        pmsg->ParseFromIstream(&is);

        S2CGetRepairFrameData_2002 *dmsg = (S2CGetRepairFrameData_2002*)&S2CGetRepairFrameData_2002::default_instance();
        dmsg->Clear();
        for(int i=0; i<pmsg->frameids_size();i++){
            unsigned int fid = pmsg->frameids(i);
            map<int,S2CServerFrameUpdate_2001*>::iterator it = m_allOperMap.find(fid);
            if(it==m_allOperMap.end()){
                //add log fatal error
                continue;
            }
            S2CServerFrameUpdate_2001* tmp = dmsg->add_framedatalist();
            tmp->CopyFrom(*it->second);
        }
        p->sendPbKcpMsg(2002, dmsg);

        auto p2001 = dmsg->framedatalist();
        p2001.Clear();
        while(p2001.ClearedCount()){
            auto tmpp = p2001.ReleaseCleared();
            roomMgr::m_inst->recyclePt2001(tmpp);
        }
    }

    void roomObj::VerifyFrame(playerObj* p, int fid, int vdata ){
        LOG("roomObj::VerifyFrame, roomid: %d pid: %d fid: %d data: %d", m_roomid,p->getpid(), fid, vdata );
    }
    void roomObj::GetCacheFrames(playerObj* p, unsigned int beginid ){
        S2CGetSyncCacheFrames_2003 *pmsg = (S2CGetSyncCacheFrames_2003 *)&S2CGetSyncCacheFrames_2003::default_instance();
        pmsg->Clear();
        for(map<int,S2CServerFrameUpdate_2001*>::iterator it = m_allOperMap.begin();it!=m_allOperMap.end(); it++){
            if(it->first<beginid){
                continue;
            }
            S2CServerFrameUpdate_2001 *ptr = pmsg->add_framedatalist();
            ptr->CopyFrom(*it->second);
        }
        p->sendPbMsg(2003, pmsg);
    }

    int roomObj::GetPIdx(playerObj* p){
        int idx = 0;
        for(map<int, playerObj*>::iterator it=m_pidPMap.begin(); it!=m_pidPMap.end(); it++){
            if(it->second->getCamp() > 1 ){
                continue;
            }
            if(p->getRid() >= it->second->getRid() ){
                idx++;
            }
        }
        return idx;
    }

    void roomObj::SvrFrameCmd(playerObj* p, int param){
        C2SFrameCommand_2000 *pmsg = roomMgr::m_inst->fetchPt();
        const int tp = 15;
        int pidx = GetPIdx(p);
        int data = (tp & 0xff) + ((pidx&0xff) << 8) + ((param&0xffff)<<16);
        pmsg->set_data( data );
        if(!m_pt2001){
            m_pt2001 = roomMgr::m_inst->fetchPt2001();
        }
        m_pt2001->mutable_cmdlist()->AddAllocated(pmsg);
    }

    playerObj* roomObj::getP(int pid){
        map<int, playerObj*>::iterator it = m_pidPMap.find(pid);
        if(it==m_pidPMap.end()){
            return NULL;
        }
        return it->second;
    }
    playerObj* roomObj::getR(unsigned long long rid){
        map<unsigned long long, playerObj*>::iterator it = m_ridPMap.find(rid);
        if(it==m_ridPMap.end()){
            return NULL;
        }
        return it->second;
    }

    void roomObj::FrameCmd(int pid, char* cobj ){
        auto p = getP(pid);
        if(!p){
            LOG("[ERROR] FrameCmd failed, pid: %d", pid);
            return;
        }

        msgObj* obj = (msgObj*) cobj;
        C2SFrameCommand_2000 *pmsg = roomMgr::m_inst->fetchPt();
        string val((char*)obj->getBodyPtr(), obj->getBodylen());
        istringstream is(val);
        pmsg->ParseFromIstream(&is);

        if(!m_pt2001) {
            m_pt2001 = roomMgr::m_inst->fetchPt2001();
        }
        m_pt2001->mutable_cmdlist()->AddAllocated(pmsg);

        LOG("roomObj::FrameCmd roomid: %d pid: %d",m_roomid, pid);
    }


    bool roomObj::IsOver(){
        if (m_startMs == 0 ){
            return false;
        }
        return !m_brun;
    }

    void roomObj::Over(bool force){
        m_endFrameId = m_frameId;
        LOG("roomObj::over roomid: %d set frameid: %d",m_roomid, m_frameId );
        if( force ){
            m_gateOver = true;
        }
    }

    void roomObj::Broadcast(int msgid, ::google::protobuf::Message* pmsg){
        m_os.clear();
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

        Broadcast((unsigned char*)m_os.str().c_str(), bsize+8, msgid);
    }

    void roomObj::Broadcast(unsigned char* buf, int size, int msgid){
        for(map<unsigned long long,playerObj*>::iterator it=m_ridPMap.begin();it!=m_ridPMap.end();it++){
            auto p = it->second;
            if(!p){
                continue;
            }
            if(p->isOff()){
                continue;
            }
            p->sendMsg( buf, size, msgid );
        }
    }

    void roomObj::BroadcastKcp(unsigned char* buf, int size){
        for(map<int,void*>::iterator it=m_pidConn.begin();it!=m_pidConn.end();it++){
            UDPConn *p = (UDPConn*)it->second;
            if(!p){
                continue;
            }
            p->Write((const char*)buf, size);
        }
    }
    void roomObj::BroadcastKcp(int msgid, ::google::protobuf::Message* pmsg){
        m_os.clear();
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
        BroadcastKcp((unsigned char*)m_os.str().c_str(), bsize+8);
    }

    void roomObj::SetReady(){
        m_brun = true;
    }

    void roomObj::Finnal(){
        //send record to gate
        G2gAllRoomFrameData *pgmsg = (G2gAllRoomFrameData *)&G2gAllRoomFrameData::default_instance();
        auto ref = pgmsg->mutable_datalist();
        pgmsg->Clear();
        pgmsg->set_frametime(m_frameTick);
        for(map<int,S2CServerFrameUpdate_2001*>::iterator it = m_allOperMap.begin(); it!=m_allOperMap.end();it++){
            ref->AddAllocated( it->second );
        }

        m_os.clear();
        m_os.str("");

        pgmsg->SerializeToOstream(&m_os);
        ref->Clear();
        while(ref->ClearedCount()){
            ref->ReleaseCleared();
        }

        tcpclientMgr::m_sInst->rpcCallGate((char*)"UpdateFrameData", 0, 0, (unsigned char*)m_os.str().c_str(), m_os.str().size());

        //release msg memory
        S2CServerFrameUpdate_2001 *p1 = NULL;
        long len = 0;
        for(map<int,S2CServerFrameUpdate_2001*>::iterator it = m_allOperMap.begin(); it!=m_allOperMap.end();it++){
            p1 = it->second;
            roomMgr::m_inst->recyclePt2001(p1);
        }
        LOG("roomid: %d Finnal bin size: %d pt2000 len: %d", m_roomid, pgmsg->ByteSize(), len);
        m_allOperMap.clear();
    }
    void roomObj::RawOver(){

        S2CMatchOver_213 *pmsg = (S2CMatchOver_213*)&S2CMatchOver_213::default_instance();
        pmsg->Clear();
        if(m_frameId>0){
            pmsg->set_overframeid(m_frameId-1);
        }else{
            pmsg->set_overframeid(m_frameId);
        }
        Broadcast(213, pmsg);
        m_brun = false;
        LOG("roomid: %d RawOver m_frameId: %d", m_roomid, m_frameId );

        for(map<unsigned long long,playerObj*>::iterator it=m_ridPMap.begin(); it!=m_ridPMap.end();it++){
            playerMgr::m_inst->RemoveP(it->first);
        }
    }


    //for mgr
    //--------------------------------------------------------------------------------
    roomMgr::roomMgr(){
        mutex = new pthread_mutex_t;
        pthread_mutex_init( mutex, NULL );
        initObjPool();
        m_lastMs = 0;
    }

    //pool funcs
    void roomMgr::initObjPool(){
        if( netServer::g_netServer->isNet() ){
            return;
        }
        for(int i=0; i<POOL_PT_2000_SIZE; i++ ){
            C2SFrameCommand_2000 *pmsg = new C2SFrameCommand_2000;
            m_pool.push(pmsg);
        }

        for(int i=0; i<POOL_PT_2001_SIZE; i++ ){
            S2CServerFrameUpdate_2001 *pmsg = new S2CServerFrameUpdate_2001;
            m_pool2001.push(pmsg);
        }

        for(int i=0; i<POOL_ROOMOBJ; i++){
            roomObj *p = new roomObj;
            m_poolRoom.push(p);
        }
    }
    void roomMgr::pushRoom(roomObj *r){
        m_poolRoom.push(r);
    }
    roomObj* roomMgr::popRoom(){
        roomObj *r = NULL;
        if(m_poolRoom.empty()){
            r = new roomObj;
            LOG("!!!!!!!!!! unexpected error, pool roomObj* was recreated");
        }else{
            r = m_poolRoom.front();
            m_poolRoom.pop();
        }
        return r;
    }

    void roomMgr::recyclePt2001(S2CServerFrameUpdate_2001* p){
        auto ref0 = p->mutable_cmdlist();
        ref0->Clear();
        while(ref0->ClearedCount()){
            auto p0 = ref0->ReleaseCleared();
            recyclePt(p0);
        }
        m_pool2001.push(p);
    }

    S2CServerFrameUpdate_2001* roomMgr::fetchPt2001(){
        S2CServerFrameUpdate_2001* pmsg = NULL;
        if( m_pool2001.size() == 0 ){
            pmsg = new S2CServerFrameUpdate_2001;
            //LOG("!!!!!!!!!! unexpected error, pool C2SFrameCommand_2001 was recreated");
        }else{
            pmsg = m_pool2001.front();
            m_pool2001.pop();
            pmsg->Clear();
        }
        return pmsg;
    }


    void roomMgr::recyclePt(C2SFrameCommand_2000* p){
        m_pool.push(p);
    }

    C2SFrameCommand_2000* roomMgr::fetchPt(){
        C2SFrameCommand_2000* pmsg = NULL;
        if( m_pool.size() == 0 ){
            pmsg = new C2SFrameCommand_2000;
            //LOG("!!!!!!!!!! unexpected error, pool C2SFrameCommand_2000 was recreated");
        }else{
            pmsg = m_pool.front();
            m_pool.pop();
            pmsg->Clear();
        }
        return pmsg;
    }

    void roomMgr::_lock(int v){
        pthread_mutex_lock(mutex);
    }
    void roomMgr::_unlock(int v){
        pthread_mutex_unlock(mutex);
    }
    void roomMgr::AppendR(roomObj *p){
        _lock(0);
        m_idMap[p->getRoomid()] = p;
        _unlock(0);
        LOG("roommgr AppendR roomid: %d", p->getRoomid());
    }
    roomObj* roomMgr::GetRoom(int roomid){
        _lock(1);
        map<int,roomObj*>::iterator it= m_idMap.find(roomid);
        if(it == m_idMap.end()){
            _unlock(1);
            return NULL;
        }
        _unlock(1);
        return it->second;
    }

    int roomMgr::Count(){
        int size = 0;
        _lock(5);
        size = m_idMap.size();
        _unlock(5);
        return size;
    }

    string roomMgr::DebugInfo(){
        stringstream ss;
        _lock(9);
        ss << " 2001 count:" << m_pool2001.size() << ",sizeof:"<< sizeof(S2CServerFrameUpdate_2001) << ",mem:" << m_pool2001.size() * sizeof(S2CServerFrameUpdate_2001) << endl;
        ss << " 2000 count:" << m_pool.size() << ",sizeof:"<< sizeof(C2SFrameCommand_2000) << ",mem:" << m_pool.size() * sizeof(C2SFrameCommand_2000) << endl;
        ss << " roomobj count:" << m_poolRoom.size() << ",sizeof:"<< sizeof(roomObj) << ",mem:" << m_poolRoom.size() * sizeof(roomObj) << endl;
        _unlock(9);
        return ss.str();
    }

    void roomMgr::UpdateFinnal(){
        _lock(8);
        roomObj* r =NULL;
        while(!m_finnalRoomQue.empty()){
            r = m_finnalRoomQue.front();
            m_finnalRoomQue.pop();
            r->Finnal();
            pushRoom(r);
        }
        _unlock(8);
    }
    void roomMgr::Update(unsigned int ms){
        if(ms-m_lastMs<1){
            return;
        }
        m_lastMs = ms;

        _lock(3);
        for(map<int,roomObj*>::iterator it= m_idMap.begin(); it!= m_idMap.end();){
            if(!it->second){
                continue;
            }
            if(it->second->Update(ms) == 1 ){
                m_finnalRoomQue.push(it->second);
                m_idMap.erase(it++);
            }else{
                it++;
            }
        }
        _unlock(3);
        UpdateFinnal();
    }
}
