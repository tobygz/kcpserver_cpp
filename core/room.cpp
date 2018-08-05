#include "room.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>


#include "../log.h"
#include "../recvBuff.h"
#include "../net.h"
#include "./pb/server.pb.h"
#include "../tcpclient.h"

#define FRAME_TICK_MS 33

using namespace pb;

namespace net{

    roomMgr* roomMgr::m_inst = new roomMgr;

    roomObj::roomObj(int roomid){
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
    }

    roomObj::~roomObj(){

    }
    void roomObj::UpdateDelta(unsigned int delta){
        m_frameTick = (delta&0xffff0000) >> 16;
        LOG("UpdateDelta roomid: %d val: %d", m_roomid, m_frameTick );
    }

    void roomObj::EnterP(playerObj* p){
        m_allRidMap[p->getRid()] = p;
        LOG("EnterP roomid: %d rid: %d camp: %d", m_roomid, p->getRid(), p->getCamp() );
    }

    void roomObj::LeaveP(playerObj* p){
        m_leaveRidMap[p->getRid()] = true;
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

        if(m_allRidMap.size()==0){
            return 0;
        }

        if( m_lastMs == 0 ){
            m_lastMs = ms;
            return 0;
        }
        m_accMs += ms - m_lastMs;
        m_lastMs = ms;
        int len = 0;
        while(m_accMs>=m_frameTick){
            m_accMs -= m_frameTick;
            S2CServerFrameUpdate_2001 *msg = new S2CServerFrameUpdate_2001;
            msg->set_frameid(m_frameId);

            for(FRAME_OPER_MAP::iterator it=m_ridOpersMap.begin(); it!= m_ridOpersMap.end(); it++){
                queue<C2SFrameCommand_2000*>* que = it->second;
                while(!que->empty()){
                    len++;
                    C2SFrameCommand_2000 *ptr = msg->add_cmdlist();
                    C2SFrameCommand_2000 *pop = que->front();
                    que->pop();
                    ptr->CopyFrom(*pop);
                }
            }
            LOG("roomobj::update ms: %u len: %d m_frameId: %d m_accMs: %d", ms, len, m_frameId, m_accMs);
            m_allOperMap[m_frameId] = msg;
            //broadcast msg
            BroadcastKcp(2001, msg);
            m_frameId++;
        }

        for(map<unsigned long long, bool>::iterator it=m_leaveRidMap.begin(); it!= m_leaveRidMap.end();it++){
            playerObj *p = playerMgr::m_inst->GetPByRid(it->first);
            m_allRidMap.erase(it->first);
            if(!p){
                continue;
            }
            S2CMatchOver_213 *pmsg = (S2CMatchOver_213*)&S2CMatchOver_213::default_instance();
            pmsg->Clear();
            if(m_frameId>0){
                pmsg->set_overframeid(m_frameId-1);
            }else{
                pmsg->set_overframeid(m_frameId);
            }
            p->sendPbMsg(213, pmsg);
            p->Offline();
        }
        m_leaveRidMap.clear();

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
                assert(false);
                continue;
            }
            S2CServerFrameUpdate_2001* tmp = dmsg->add_framedatalist();
            tmp->CopyFrom(*it->second);
        }
        p->sendPbKcpMsg(2002, dmsg);
    }

    void roomObj::GetCacheFrames(playerObj* p, unsigned int beginid ){
        S2CGetSyncCacheFrames_2003 *pmsg = new S2CGetSyncCacheFrames_2003;
        for(map<int,S2CServerFrameUpdate_2001*>::iterator it = m_allOperMap.begin();it!=m_allOperMap.end(); it++){
            if(it->first<beginid){
                continue;
            }
            S2CServerFrameUpdate_2001 *ptr = pmsg->add_framedatalist();
            ptr->CopyFrom(*it->second);
        }
        p->sendPbMsg(2003, pmsg);
    }
    void roomObj::FrameCmd(playerObj* p, char* cobj ){
        msgObj* obj = (msgObj*) cobj;
        C2SFrameCommand_2000 *pmsg = new C2SFrameCommand_2000;
        string val((char*)obj->getBodyPtr(), obj->getBodylen());
        istringstream is(val);
        pmsg->ParseFromIstream(&is);

        FRAME_OPER_MAP::iterator it = m_ridOpersMap.find(p->getRid());
        if(it == m_ridOpersMap.end()){
            queue<C2SFrameCommand_2000*>* pque = new queue<C2SFrameCommand_2000*>;
            pque->push(pmsg);
            m_ridOpersMap[p->getRid()] = pque;
        }else{
            it->second->push(pmsg);
        }
    }


    bool roomObj::IsOver(){
        if (m_startMs == 0 ){
            return false;
        }
        return !m_brun;
    }

    bool roomObj::Over(bool force){
        m_endFrameId = m_frameId;
        LOG("roomObj::over set frameid: %d", m_frameId );
        if( force ){
            m_gateOver = true;
        }
    }

    void roomObj::Broadcast(int msgid, ::google::protobuf::Message* msg){
        m_os.clear();
        m_os.str("");
        msg->SerializeToOstream(&m_os);
        int len = m_os.str().size();
        memcpy(m_buf, &len, 4);
        memcpy(m_buf+4, &msgid, 4);
        memcpy(m_buf+8, (unsigned char*)m_os.str().c_str(), len );

        Broadcast((unsigned char*)m_buf, len+8, msgid);
    }

    void roomObj::Broadcast(unsigned char* buf, int size, int msgid){
        for(map<unsigned long long,bool>::iterator it=m_allRidMap.begin();it!=m_allRidMap.end();it++){
            playerObj *player = playerMgr::m_inst->GetPByRid(it->first);
            if(!player){
                continue;
            }
            if(player->isOff()){
                continue;
            }
            player->sendMsg( buf, size, msgid );
        }
    }

    void roomObj::BroadcastKcp(unsigned char* buf, int size){
        for(map<unsigned long long,bool>::iterator it=m_allRidMap.begin();it!=m_allRidMap.end();it++){
            playerObj *player = playerMgr::m_inst->GetPByRid(it->first);
            if(!player){
                continue;
            }
            if(player->isOff()){
                continue;
            }
            player->sendKcpMsg( buf, size );
        }
    }
    void roomObj::BroadcastKcp(int msgid, ::google::protobuf::Message* msg){
        m_os.clear();
        m_os.str("");
        msg->SerializeToOstream(&m_os);
        int len = m_os.str().size();
        memcpy(m_buf, &len, 4);
        memcpy(m_buf+4, &msgid, 4);
        memcpy(m_buf+8, (unsigned char*)m_os.str().c_str(), len );

        BroadcastKcp((unsigned char*)m_buf, len+8);
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
        LOG("RawOver send frameid: %d msg_frameid: %d", m_frameId, pmsg->overframeid());

        //send record to gate
        G2gAllRoomFrameData *pgmsg = (G2gAllRoomFrameData *)&G2gAllRoomFrameData::default_instance();
        pgmsg->Clear();
        pgmsg->set_frametime(m_frameTick);
        for(map<int,S2CServerFrameUpdate_2001*>::iterator it = m_allOperMap.begin(); it!=m_allOperMap.end();it++){
            S2CServerFrameUpdate_2001 *ptmp = pgmsg->add_datalist();
            ptmp->CopyFrom(*it->second);
        }

        m_os.clear();
        m_os.str("");
        pgmsg->SerializeToOstream(&m_os);

        tcpclientMgr::m_sInst->rpcCallGate((char*)"UpdateFrameData", 0, 0, (unsigned char*)m_os.str().c_str(), m_os.str().size());

        //release msg memory
        for(map<int,S2CServerFrameUpdate_2001*>::iterator it = m_allOperMap.begin(); it!=m_allOperMap.end();it++){
            delete it->second;
        }
        m_allOperMap.clear();


        for(map<unsigned long long,bool>::iterator it=m_allRidMap.begin(); it!=m_allRidMap.end();it++){
            playerMgr::m_inst->RemoveP(it->first);
        }
        //roomMgr::m_inst->DelRoom(m_roomid);
    }


    //for mgr
    //--------------------------------------------------------------------------------
    roomMgr::roomMgr(){
        mutex = new pthread_mutex_t;
        pthread_mutex_init( mutex, NULL );
    }
    void roomMgr::_lock(int v){
        //LOG("roomMgr lock v: %d", v );
        pthread_mutex_lock(mutex);
    }
    void roomMgr::_unlock(int v){
        //LOG("roomMgr unlock v: %d", v );
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
    void roomMgr::DelRoom(int roomid){
        _lock(2);
        map<int,roomObj*>::iterator it= m_idMap.find(roomid);
        if(it == m_idMap.end()){
            _unlock(2);
            return ;
        }
        m_idMap.erase(it);
        _unlock(2);
        LOG("roommgr DelRoom roomid: %d", roomid );
    }

    void roomMgr::Update(unsigned int ms){
        queue<int> dlst;
        _lock(3);
        for(map<int,roomObj*>::iterator it= m_idMap.begin(); it!= m_idMap.end(); it++ ){
            if(!it->second){
                continue;
            }
            if(it->second->Update(ms) == 1 ){
                dlst.push(it->first);
            }
        }
        _unlock(3);
        while(!dlst.empty()){
            int id = dlst.front();
            dlst.pop();
            DelRoom(id);
        }
    }
}
