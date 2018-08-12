
#include "qps.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "connmgr.h"

#include "log.h"
#include "net.h"
#include "./kcpsess/sessServer.h"
#include "./core/player.h"

using namespace std;
namespace net{
    qpsMgr* qpsMgr::g_pQpsMgr = new qpsMgr;

    pthread_mutex_t *mutexQps ; 

    long long getms(){
        struct timeval te; 
        gettimeofday(&te, NULL); // get current time
        long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
        return milliseconds;
    }
    int getsec(){
        struct timeval te; 
        gettimeofday(&te, NULL); // get current time
        long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
        return int(milliseconds/1000);
    }

    qpsMgr::qpsMgr(){
        m_lastMs = 0;
        mutexQps = new pthread_mutex_t;
        pthread_mutex_init( mutexQps, NULL );

        //create 1 qpsobj
        //addQps(1,(char*)"network");//for network
        //addQps(2,(char*)"mainloop");//for mainloop
        //addQps(3,(char*)"readfd");//for readfd
        //addQps(4,(char*)"dealrecv");//for recvBuff process
        if( netServer::g_netServer->isNet() ){
            addQps(1,(char*)"rpcRecv");
            addQps(2,(char*)"rpcSend");
            addQps(3,(char*)"cliSend");
            addQps(4,(char*)"cliRecv");
        }else{
            addQps(1,(char*)"mainCt");
            addQps(2,(char*)"rpcTps");
            addQps(3,(char*)"kcpSend");
            addQps(4,(char*)"kcpRecv");
            /*
            addQps(5,(char*)"m1");
            addQps(6,(char*)"m2");
            addQps(7,(char*)"m3");
            */
        }
    }

    void qpsMgr::updateQps(int id, int _size){
        if(_size==0){
            return;
        }
        pthread_mutex_lock(mutexQps);
        map<int,qpsObj*>::iterator it = m_qpsMap.find(id);
        if( it == m_qpsMap.end() ){
            pthread_mutex_unlock(mutexQps);
            return;
        }
        pthread_mutex_unlock(mutexQps);
        qpsObj* tmp = (qpsObj*)it->second;
        tmp->count++;
        tmp->size += _size;
    }

    void qpsMgr::dumpQpsInfo(){
        long long nowMs = getms();
        if(nowMs - m_lastMs<1000){
            return;
        }
        m_lastMs = nowMs;
        memset(m_debugInfo,0,sizeof m_debugInfo);
        pthread_mutex_lock(mutexQps);
        map<int,qpsObj*>::iterator iter;
        float mval=0;
        for( iter=m_qpsMap.begin(); iter!=m_qpsMap.end(); iter++){
            qpsObj* tmp = (qpsObj*) iter->second;
            
            if(tmp->size>1024*1024){
                mval = tmp->size/1024/1024.0;
                sprintf(m_debugInfo, "%s[t:%s c:%d s:%.2f]", m_debugInfo, tmp->info, tmp->count, mval );
            }else{
                sprintf(m_debugInfo, "%s[t:%s c:%d s:%lld]", m_debugInfo, tmp->info, tmp->count, tmp->size );
                /*
                if(tmp->size == 0 ){
                    sprintf(m_debugInfo, "%s[t:%s c:%d]", m_debugInfo, tmp->info, tmp->count );
                }else{
                    sprintf(m_debugInfo, "%s[t:%s c:%d s:%lld]", m_debugInfo, tmp->info, tmp->count, tmp->size );
                }
                */
            }
            tmp->Reset();
        }
        if( netServer::g_netServer->isNet() ){
            sprintf(m_debugInfo, "%s [online: %d]", m_debugInfo, connObjMgr::g_pConnMgr->GetOnline());
        }else{
            sprintf(m_debugInfo, "%s [online: %d p: %d]", m_debugInfo, KCPServer::m_sInst->getCount(), playerMgr::m_inst->GetCount());
        }

        pthread_mutex_unlock(mutexQps);
        LOG("QPS %s", m_debugInfo );
    }
}
