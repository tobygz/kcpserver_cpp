#include "room.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>


#include "../log.h"
#include "../recvBuff.h"
#include "../net.h"

#define FRAME_TICK_MS 33

namespace net{

    roomMgr* roomMgr::m_inst = new roomMgr;

    roomObj::roomObj(int roomid){
        m_roomid = roomid;
        m_frameId = 0;
        m_endFrameId = 2147483648;
        m_brun = false;
        m_bornMs = 0;
        m_startMs = 0;
        m_accMs = 0;
    }

    void roomObj::EnterP(playerObj* p){
        m_allRidMap[p->getRid()] = p;
        LOG("EnterP roomid: %d rid: %d camp: %d", m_roomid, p->getRid(), p->getCamp() );
    }

    void roomObj::Update(unsigned int ms){
        if(!m_brun){
            return;
        }

        if(m_allRidMap.size()==0){
            return;
        }

        if( m_lastMs == 0 ){
            m_lastMs = ms;
            return;
        }
        m_accMs += ms - m_lastMs;
        m_lastMs = ms;
        int len = 0;
        while(m_accMs>=FRAME_TICK_MS){
            m_accMs -= FRAME_TICK_MS;
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
            LOG("roomobj::update ms: %u len: %d m_frameId: %d", ms, len, m_frameId);
            m_allOperMap[m_frameId] = msg;
            //broadcast msg
            BroadcastKcp(2001, msg);
            //delete msg;

            m_frameId++;
        }

        if(m_frameId>m_endFrameId && m_endFrameId != 2147483648){
            RawOver();
        }
    }


    void roomObj::GetFramesData(playerObj* p, char* cobj ){
        msgObj* obj = (msgObj*) cobj;
        C2SGetRepairFrameData_2002 *pmsg = new C2SGetRepairFrameData_2002;
        string val((char*)obj->getBodyPtr(), obj->getBodylen());
        istringstream is(val);
        pmsg->ParseFromIstream(&is);

        S2CGetRepairFrameData_2002 *dmsg = new S2CGetRepairFrameData_2002;
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

    bool roomObj::Over(){
        m_endFrameId = m_frameId;
    }

    void roomObj::Broadcast(::google::protobuf::Message* msg){
        m_os.clear();
        m_os.str("");
        msg->SerializeToOstream(&m_os);
        Broadcast((unsigned char*)m_os.str().c_str(), m_os.str().size());
    }
    void roomObj::Broadcast(unsigned char* buf, int size){
        for(map<unsigned long long,bool>::iterator it=m_allRidMap.begin();it!=m_allRidMap.end();it++){
            playerObj *player = playerMgr::m_inst->GetPByRid(it->first);
            if(!player){
                continue;
            }
            if(player->isOff()){
                continue;
            }
            player->sendMsg( buf, size );
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
        char *p = new char[len + 8 ];
        memcpy(p, &len, 4);
        memcpy(p+4, &msgid, 4);
        memcpy(p+8, (unsigned char*)m_os.str().c_str(), len );

        BroadcastKcp((unsigned char*)p, len+8);
        delete[] p;
    }

    void roomObj::RawOver(){
        S2CMatchOver_213 *pmsg = new S2CMatchOver_213;
        pmsg->set_overframeid(m_frameId-1);
        //Broadcast(pmsg);
        //send all 213
        m_brun = false;
        //reset all kcp conn
        //send record to gate
        //delete room

        for(map<unsigned long long,bool>::iterator it=m_allRidMap.begin(); it!=m_allRidMap.end();it++){
            playerMgr::m_inst->RemoveP(it->first);
        }
        roomMgr::m_inst->DelRoom(m_roomid);
    }


    //for mgr
    //--------------------------------------------------------------------------------
    roomMgr::roomMgr(){
        mutex = new pthread_mutex_t;
        pthread_mutex_init( mutex, NULL );
    }
    void roomMgr::AppendR(roomObj *p){
        pthread_mutex_lock(mutex);
        m_idMap[p->getRoomid()] = p;
        pthread_mutex_unlock(mutex);
        LOG("roommgr AppendR roomid: %d", p->getRoomid());
    }
    roomObj* roomMgr::GetRoom(int roomid){
        pthread_mutex_lock(mutex);
        map<int,roomObj*>::iterator it= m_idMap.find(roomid);
        if(it == m_idMap.end()){
            return NULL;
        }
        pthread_mutex_unlock(mutex);
        return it->second;
    }
    void roomMgr::DelRoom(int roomid){
        pthread_mutex_lock(mutex);
        map<int,roomObj*>::iterator it= m_idMap.find(roomid);
        if(it == m_idMap.end()){
            return ;
        }
        m_idMap.erase(it);
        pthread_mutex_unlock(mutex);
        LOG("roommgr DelRoom roomid: %d", roomid );
    }

    void roomMgr::Update(unsigned int ms){
        pthread_mutex_lock(mutex);
        for(map<int,roomObj*>::iterator it= m_idMap.begin(); it!= m_idMap.end(); it++ ){
            if(!it->second){
                continue;
            }
            it->second->Update(ms);
        }
        pthread_mutex_unlock(mutex);
    }
}
