
#include "rpchandle.h"


#include <stdio.h>
#include <string.h>

#include "net.h"
#include "recvBuff.h"
#include "connmgr.h"
#include "log.h"

#include "./core/player.h"
#include "./core/room.h"
#include "qps.h"

//for net
#define RPC_PING "ping"
#define RPC_PushMsg2Client "PushMsg2Client"
#define RPC_PushMsg2ClientAll "PushMsg2ClientAll"
#define RPC_ForceCloseCliConn "ForceCloseCliConn"
#define RPC_RegGamePid "RegGamePid"

//for game
#define RPC_GAME_NewRoom  "NewRoom"
#define RPC_GAME_OverRoom  "OverRoom"
#define RPC_GAME_OfflineP  "OfflineP"
#define RPC_GAME_EnterP  "EnterP"
#define RPC_GAME_LeaveP  "LeaveP"
#define RPC_GAME_Msg2game  "Msg2game"
#define RPC_GAME_SvrFrameCmd  "SvrFrameCmd"
#define RPC_GAME_AllPReady  "AllPReady"
#define RPC_GAME_TakeProxy "TakeProxy"

#define RPC_API_2000 "2000"
#define RPC_API_2002 "2002"
#define RPC_API_2003 "2003"
#define RPC_API_2004 "2004"


namespace net{


    rpcNetHandle* rpcNetHandle::m_pInst = new rpcNetHandle();
    rpcGameHandle* rpcGameHandle::m_pInst = new rpcGameHandle();

    void rpcNetHandle::process(rpcObj* pobj){
        map<string,PTRFUN>::iterator it ;
        const char* ptar = (const char*)pobj->getTarget();
        string key(ptar);
        it = m_mapHandler.find(key);			
        if( it == m_mapHandler.end()) {
            LOG("[ERROR] rpcNetHandle invalied rpc name: %s", pobj->getTarget() );
            return;
        }
        LOG("[DEBUG] process rpc name: %s", pobj->getTarget() );

        if(it!= m_mapHandler.end()){
            (this->*it->second)(pobj);
        }
    }

    rpcNetHandle::rpcNetHandle(){
        init();
    }
    void rpcNetHandle::init(){
        //for net_server
        m_mapHandler[ string( RPC_PING )] = &net::rpcNetHandle::onPing;
        m_mapHandler[ string( RPC_PushMsg2Client) ] = &net::rpcNetHandle::onPushMsg2Client;
        m_mapHandler[string( RPC_PushMsg2ClientAll) ] = &net::rpcNetHandle::onPushMsg2ClientAll;
        m_mapHandler[string( RPC_ForceCloseCliConn) ] = &net::rpcNetHandle::onForceCloseCliConn;
        m_mapHandler[string( RPC_RegGamePid) ] = &net::rpcNetHandle::onRegGamePid;

    }

    int rpcNetHandle::onPing(rpcObj*){
        LOG("called onPing");
    }

    int rpcNetHandle::onPushMsg2Client(rpcObj* pobj){
        LOG("[RPC] called onPushMsg2Client pid: %d bodylen: %d msgid: %d", pobj->getPid(), pobj->getBodylen(), pobj->getMsgid());
        connObjMgr::g_pConnMgr->SendMsg( (unsigned int)pobj->getPid(), pobj->getBodyPtr(), pobj->getBodylen() );
    }

    int rpcNetHandle::onPushMsg2ClientAll(rpcObj* pobj){
        //LOG("[RPC] called onPushMsg2ClientAll");
        connObjMgr::g_pConnMgr->SendMsgAll( pobj->getBodyPtr(), pobj->getBodylen() );
    }

    int rpcNetHandle::onForceCloseCliConn(rpcObj* pobj){
        //LOG("[RPC] called onForceCloseCliConn");
        connObjMgr::g_pConnMgr->DelConn((unsigned int)pobj->getPid());
    }

    int rpcNetHandle::onRegGamePid(rpcObj* pobj){
        //LOG("[RPC] called onRegGamePid pid: %ul name: %s", pobj->getPid(), pobj->getParam());
        connObjMgr::g_pConnMgr->RegGamePid((unsigned int)pobj->getPid(), (char*)pobj->getParam());
    }

    //for gamehandler
    //------------------------------------------------------------------------------------------
    rpcGameHandle::rpcGameHandle(){
        init();
    }
    void rpcGameHandle::init(){
        m_mapHandler[string( RPC_GAME_NewRoom)] = &net::rpcGameHandle::NewRoom;
        m_mapHandler[string( RPC_GAME_OverRoom)] = &net::rpcGameHandle::OverRoom;
        m_mapHandler[string( RPC_GAME_OfflineP)] = &net::rpcGameHandle::OfflineP;
        m_mapHandler[string( RPC_GAME_EnterP)] = &net::rpcGameHandle::EnterP;
        m_mapHandler[string( RPC_GAME_LeaveP )] = &net::rpcGameHandle::LeaveP;
        m_mapHandler[string( RPC_GAME_SvrFrameCmd)] = &net::rpcGameHandle::SvrFrameCmd;
        m_mapHandler[string( RPC_GAME_AllPReady)] = &net::rpcGameHandle::AllPReady;
        m_mapHandler[string( RPC_GAME_Msg2game)] = &net::rpcGameHandle::Msg2game;
        m_mapHandler[string( RPC_GAME_TakeProxy)] = &net::rpcGameHandle::TakeProxy;


        //customer
        m_mapHandler[string(RPC_API_2000 )] = &net::rpcGameHandle::Api_2000;
        m_mapHandler[string(RPC_API_2002 )] = &net::rpcGameHandle::Api_2002;
        m_mapHandler[string(RPC_API_2003 )] = &net::rpcGameHandle::Api_2003;
        m_mapHandler[string(RPC_API_2004 )] = &net::rpcGameHandle::Api_2004;
    }


    int rpcGameHandle::NewRoom(rpcObj* _obj){ 
        roomObj *room = new roomObj( int( _obj->getPid()) );
        string binstr((char*)_obj->getBodyPtr(), _obj->getBodylen());
        LOG("[NEWROOM] binstr: %s", binstr.c_str());
        const int p_size=16;
        char *svec[16];
        //vector<char*> svec(p_size);
        char* ptk = strtok((char*)binstr.c_str(), "|");
        int i = 0, j=0, k=0, f=0;
        while(ptk){
            svec[i++] = ptk;
            ptk = strtok(NULL,"|");
        }
        //assert(svec.size()==4);
        //for pid ary
        ptk = strtok(svec[0],",");
        //vector<int> pidvec(p_size);
        int pidvec[16];
        int ival=0;
        unsigned long long lval = 0;
        while(ptk){
            pidvec[j++] = atoi(ptk);
            ptk = strtok(NULL,",");
        }
        //for rid ary
        //vector<unsigned long long> ridvec(p_size);
        unsigned long long ridvec[16];
        ptk = strtok(svec[1],",");
        while(ptk){
            ridvec[k++] = atoll(ptk);
            ptk = strtok(NULL,",");
        }
        //for camp ary
        //vector<int> campvec(p_size);
        int campvec[16];
        ptk = strtok(svec[3],",");
        while(ptk){
            campvec[f++] = atoi(ptk);
            ptk = strtok(NULL,",");
        }
        //assert(pidvec.size() == ridvec.size());
        //assert(campvec.size() == ridvec.size());

        //for delta
        unsigned int delta = atoi(svec[2]);
        room->UpdateDelta(delta);

        //LOG("i: %d j: %d k: %d f: %d\n", i,j,k,f);
        //LOG("i: %d j: %d k: %d f: %d\n", i,j,k,f);
        if( j==k && k == f ){
            for(int jj=0;jj<j; jj++){
                playerObj *p = new playerObj(pidvec[jj], ridvec[jj], room->getRoomid());
                playerMgr::m_inst->AppendP(p);
                p->setCamp(campvec[jj]);
                room->EnterP(p);
            }
            roomMgr::m_inst->AppendR( room );
            LOG("called NewRoom succ, roomid: %d binstr: %s", room->getRoomid(), binstr.c_str());  
        }else{
            delete room;
            LOG("[ERROR] called NewRoom failed,roomid: %d binstr: %s",room->getRoomid(), binstr.c_str());  
        }

        return 0;
    }
    int rpcGameHandle::OverRoom(rpcObj* _obj){ 
        roomObj *p = roomMgr::m_inst->GetRoom( int(_obj->getPid()) );
        if(!p){
            LOG("[ERROR] OverRoom failed, room is nil roomid: %ld", _obj->getPid() );  
            return 1;
        }
        bool force = false;
        if( _obj->getMsgid() == 1 ){
            force = true;
        }
        p->Over(force);
        LOG("[INFO] OverRoom succ, roomid: %ld", _obj->getPid() );  
        return 0;
    }
    int rpcGameHandle::OfflineP(rpcObj* _obj){ 
        playerObj *p = playerMgr::m_inst->GetP( _obj->getPid() );
        if(!p){
            LOG("[ERROR] OfflineP failed, player is nil rid: %ld", _obj->getPid() );  
            return 1;
        }
        p->Offline();
        LOG("[INFO] Offlinep succ, rid: %ld", _obj->getPid() );  
        return 0;
    }

    int rpcGameHandle::LeaveP(rpcObj* _obj){ 
        playerObj *p = playerMgr::m_inst->GetP( _obj->getPid() );
        if(!p){
            LOG("[ERROR] LeaveP failed, player is nil rid: %ld", _obj->getPid() );  
            return 1;
        }

        roomObj *room = roomMgr::m_inst->GetRoom( p->getRoomid());
        if(!room){
            LOG("[ERROR] LeaveP failed, room is nil roomid: %d pid: %d rid: %ld ", p->getRoomid(), p->getpid(), p->getRid());
            return -1;
        }

        room->LeaveP( p );
        LOG("[INFO] LeaveP succ, rid: %ld roomid: %d", _obj->getPid(), p->getRoomid() );  
        return 0;
    }

    int rpcGameHandle::EnterP(rpcObj* _obj){ 
        string binstr((char*)_obj->getBodyPtr(), _obj->getBodylen());
        char* ptk = strtok((char*)binstr.c_str(), ",");
        int i=0;
        int pid=0,roomid=0,camp=0;
        unsigned long long rid = 0;
        while(ptk){
            if(i==0){
                rid = atoll(ptk);
            }else if( i==1 ){
                pid = atoi(ptk);
            }else if(i==2){
                roomid = atoi(ptk);
            }else if(i==3){
                camp = atoi(ptk);
            }
            i++;
            ptk = strtok(NULL,",");
        }
        assert( rid != 0);
        assert( pid != 0);
        assert( roomid != 0);
        roomObj *room = roomMgr::m_inst->GetRoom( roomid );
        if(!room){
            LOG("[ERROR] enterP failed, room is nil roomid: %d pid: %d rid: %ld ", roomid, pid, rid );
            return -1;
        }
        playerObj* p = playerMgr::m_inst->GetP( rid );
        if(!p){
            LOG("[ERROR] enterP failed, p is nil roomid: %d pid: %d rid: %ld ", roomid, pid, rid );
            return -2;
        }
        room->EnterP(p);
        LOG("[INFO] enterP succ, roomid: %d pid: %d rid: %ld ", roomid, pid, rid );
        return 0;
    }
    int rpcGameHandle::Msg2game(rpcObj* pobj){ 
        LOG("called Msg2game");  
        //for frame sync
        map<string,PTRFUN>::iterator it ;
        int msgid = pobj->getMsgid();
        char info[16] = {0};
        sprintf(info, "%d", pobj->getMsgid());
        string key(info);
        it = m_mapHandler.find(key);			
        if( it == m_mapHandler.end()) {
            LOG("[ERROR] invalied api name: %s", info);
            return -1;
        }
        (this->*it->second)(pobj);
        return 0;
    }

    //process kcp request
    void rpcGameHandle::process(msgObj* obj, int pid, int sessid){
        if( obj->getMsgid() != 2000 ){
            LOG("[ERROR] rpcGameHandle::process msgid: %d", obj->getMsgid());
        }
        if(obj->getMsgid()==2000){
            //process 2000
            playerObj *p = playerMgr::m_inst->GetP( pid );
            if(!p){
                LOG("[ERROR] Api_2000 failed, player is nil rid: %ld", pid );
                return ;
            }

            roomObj *r = roomMgr::m_inst->GetRoom( p->getRoomid());
            if(!r){
                LOG("[ERROR] Api_2000 failed, room is nil roomid: %ld", p->getRoomid()); 
                return ;
            }

            r->FrameCmd(p, (char*)obj);            
        }else if( obj->getMsgid() == 2004 ){
            //process 2004
            playerObj *p = playerMgr::m_inst->GetP( pid );
            if(!p){
                LOG("[ERROR] Api_2004 failed, player is nil rid: %ld", pid );
                return ;
            }
            roomObj *r = roomMgr::m_inst->GetRoom( p->getRoomid());
            if(!r){
                LOG("[ERROR] Api_2004 failed, room is nil roomid: %ld", p->getRoomid()); 
                return ;
            }
            p->setSessid(sessid);
            r->EnterP(p);            
        }else if( obj->getMsgid() == 2002 ){
            //process 2002
            playerObj *p = playerMgr::m_inst->GetP( pid );
            if(!p){
                LOG("[ERROR] Api_2002 failed, player is nil rid: %ld", pid );
                return ;
            }
            roomObj *r = roomMgr::m_inst->GetRoom( p->getRoomid());
            if(!r){
                LOG("[ERROR] Api_2002 failed, room is nil roomid: %ld", p->getRoomid()); 
                return ;
            }
            r->GetFramesData( p,(char*) obj );
        }else{
            LOG("[ERROR] invalid called, rpcGameHandle::process msgid: %d", obj->getMsgid());
        }
    }

    int rpcGameHandle::Api_2000(rpcObj* _obj){ 
        /*
           playerObj *p = playerMgr::m_inst->GetP( _obj->getPid() );
           if(!p){
           LOG("[ERROR] Api_2000 failed, player is nil rid: %ld", _obj->getPid() );  
           return 1;
           }
           roomObj *r = roomMgr::m_inst->GetRoom( p->getRoomid());
           if(!r){
           LOG("[ERROR] Api_2000 failed, room is nil roomid: %ld", p->getRoomid()); 
           return 2;
           }
           r->FrameCmd(p, _obj);
           */
        LOG("called Api_2000");  
        return 0;
    }
    int rpcGameHandle::Api_2002(rpcObj* _obj){ 
        LOG("called Api_2002");  
        return 0;

    }
    int rpcGameHandle::Api_2003(rpcObj* _obj){ 
        LOG("called Api_2003");  
        playerObj *p = playerMgr::m_inst->GetP( _obj->getPid() );
        if(!p){
            LOG("[ERROR] Api_2003 failed, player is nil rid: %ld", _obj->getPid() );  
            return 1;
        }
        roomObj *r = roomMgr::m_inst->GetRoom( p->getRoomid());
        if(!r){
            p->sendComRetMsg(2003,1);
            return 1;
        }
        if(r->IsOver()){
            p->sendComRetMsg(2003,2);
            return 2;
        }
        C2SGetSyncCacheFrames_2003 *pmsg = (C2SGetSyncCacheFrames_2003 *)&C2SGetSyncCacheFrames_2003::default_instance();
        string val((char*)_obj->getBodyPtr(), _obj->getBodylen());
        istringstream is(val);
        pmsg->ParseFromIstream(&is);
        r->GetCacheFrames(p, pmsg->beginframeid());
        LOG("called Api_2003 rid: %d roomid: %d beginid: %d",p->getRid(), r->getRoomid(), pmsg->beginframeid() );  
        return 0;
    }
    int rpcGameHandle::Api_2004(rpcObj* _obj){ 
        LOG("called Api_2004");  
        /*
           playerObj *p = playerMgr::m_inst->GetP( _obj->getPid() );
           if(!p){
           LOG("[ERROR] Api_2000 failed, player is nil rid: %ld", _obj->getPid() );  
           return 1;
           }
           roomObj *r = roomMgr::m_inst->GetRoom( p->getRoomid());
           if(!r){
           LOG("[ERROR] Api_2000 failed, room is nil roomid: %ld", p->getRoomid()); 
           return 2;
           }
        //todo add fd
        p->setFd(0);
        r->EnterP(p);
        */
        return 0;
    }

    int rpcGameHandle::SvrFrameCmd(rpcObj* _obj){ 
        LOG("called SvrFrameCmd");  
        return 0;
    }
    int rpcGameHandle::AllPReady(rpcObj* _obj){
        roomObj *p = roomMgr::m_inst->GetRoom( int(_obj->getPid()) );
        if(!p){
            LOG("[ERROR] AllPReady failed, room is nil roomid: %ld", _obj->getPid() );  
            return 1;
        }
        p->SetReady();
        LOG("[INFO] AllPReady succ, roomid: %ld", _obj->getPid() );  
        return 0;
    }

    //process net\gate rpc request; from main thread
    void rpcGameHandle::process(rpcObj* pobj){
        map<string,PTRFUN>::iterator it ;
        const char* ptar = (const char*)pobj->getTarget();
        string key(ptar);
        it = m_mapHandler.find(key);			
        if( it == m_mapHandler.end()) {
            LOG("[ERROR] rpcGameHandle invalied rpc name: %s", pobj->getTarget() );
            return;
        }
        LOG("[DEBUG] process rpc name: %s", pobj->getTarget() );

        if(it!= m_mapHandler.end()){
            (this->*it->second)(pobj);
        }
    }

}

