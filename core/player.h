#ifndef __player_header__
#define __player_header__

#include <map>
#include <sstream>
#include "./pb/login.pb.h"

using namespace std;
using namespace pb;

namespace net{

    class playerObj{
        protected:
            int m_pid;
            //int m_fd;
            int m_sessid;
            unsigned long long m_rid;
            int m_roomid;
            bool m_off;
            int m_camp;
            ostringstream m_os;
            istringstream m_is;
        public:
            playerObj(int pid, unsigned long long rid, int roomid );
            unsigned long long getRid(){return m_rid;}
            void setCamp(int val){ m_camp = val; }
            int getCamp(){ return m_camp;}
            void setRoomid(int val){ m_roomid= val; }
            int getRoomid(){ return m_roomid ; }
            int getpid(){ return m_pid; }
            int getSessid()const{ return m_sessid; }
            void setSessid(int _id);

            //tcp
            void sendComRetMsg(unsigned int msgid, int ret);
            void sendMsg(unsigned char*, size_t size, int msgid=0);
            void sendPbMsg(unsigned int msgid, ::google::protobuf::Message*);

            void sendKcpMsg(unsigned char*, size_t size);
            void sendPbKcpMsg(unsigned int msgid, ::google::protobuf::Message*);


            bool isOff(){ return m_off; }
            void Offline();
    };

    class playerMgr{
        protected:
            map<unsigned long long,playerObj*> m_ridPMap;
        public:
            static playerMgr* m_inst;
            playerMgr(){}

            void AppendP(int pid, unsigned long long rid, char *acc , int roomid );
            void AppendP(playerObj*);
            playerObj* GetPByRid(unsigned long long rid);
            playerObj* GetP(int pid);
            void RemoveP(unsigned long long );
            //playerObj* GetPByPid(int pid);
            //void RemovePByPid(int pid );
    };
}
#endif
