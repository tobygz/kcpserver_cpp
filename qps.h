#ifndef __qps_header__
#define __qps_header__

#include <map>
#include <string.h>

namespace net{

    long long getms(); 
    int getsec(); 

    class qpsObj{
        public:
            qpsObj(int _id, char *val){
                id = _id;
                strcpy(info, val);
                Reset();
            }
            int id;
            char info[512];
            int count;
            unsigned long long size;
            void Reset(){
                count = 0;
                size = 0;
            }
    };

    class qpsMgr{

        std::map<int,qpsObj*> m_qpsMap; 
        long long m_lastMs;
        char m_debugInfo[1024];

        public:
        qpsMgr();

        void addQps(int id, char* val){
            qpsObj *pobj = new qpsObj(id, val);
            m_qpsMap[id] = pobj;
        }
        void updateQps(int id, int _size);
        void dumpQpsInfo();

        static qpsMgr* g_pQpsMgr ;
    };
}
#endif
