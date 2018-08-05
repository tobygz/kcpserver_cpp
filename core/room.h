#ifndef __room_header__
#define __room_header__

#include <map>
#include <queue>
#include <sstream>

#include "player.h"
#include "./pb/login.pb.h"

using namespace std;
using namespace pb;


namespace net{

    typedef map<unsigned long long, queue<C2SFrameCommand_2000*>* > FRAME_OPER_MAP;
    class roomObj{
        protected:
            int m_roomid;
            int m_frameId;
            map<unsigned long long,bool> m_allRidMap;

            ostringstream m_os;
            istringstream m_is;

            unsigned char m_buf[128];
            //runtime vars
            unsigned int m_endFrameId;
            bool m_brun;
            unsigned int m_bornMs;
            unsigned int m_startMs;
            unsigned int m_lastMs;
            unsigned int m_accMs; //ÀÛ»ýms

            unsigned int m_frameTick;
            bool m_gateOver;
            
            //for record datas
            FRAME_OPER_MAP m_ridOpersMap;
            map<int,S2CServerFrameUpdate_2001*> m_allOperMap;
            map<unsigned long long, bool> m_leaveRidMap;

            //for verify

        public:
            ~roomObj();
            roomObj(int roomid);
            void EnterP(playerObj* p);
            void LeaveP(playerObj* p);
            int Update(unsigned int ms);
            void FrameCmd(playerObj* p, char* );
            void GetFramesData(playerObj* p, char* );

            void GetCacheFrames(playerObj* p, unsigned int beginid );

            void Broadcast(unsigned char*, int size, int msgid);
            void Broadcast(int msgid,::google::protobuf::Message*);

            void BroadcastKcp(unsigned char*, int size);
            void BroadcastKcp(int msgid, ::google::protobuf::Message*);
            bool IsOver();
            bool Over(bool force=true);
            bool SetReady(){ m_brun = true; }
            void RawOver();

            void UpdateDelta(unsigned int );
            int getRoomid(){ return m_roomid; }
    };

    class roomMgr{
        protected:
            map<int,roomObj*> m_idMap;
            pthread_mutex_t *mutex ; 
            ostringstream m_os;
        public:
            static roomMgr* m_inst;
            roomMgr();
            void AppendR(roomObj *p);
            roomObj* GetRoom(int roomid);
            void DelRoom(int roomid);
            void Update(unsigned int ms);
            void _lock(int );
            void _unlock(int );
    };

}
#endif
