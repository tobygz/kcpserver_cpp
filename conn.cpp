#include "connmgr.h"
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>


#include "qps.h"
#include "net.h"
#include "tcpclient.h"
#include "log.h"
#include "rpchandle.h"

namespace net{

    pthread_mutex_t *mutexRecv ; 
    connRpcObj* connRpcObj::m_inst = NULL;

    void connObj::OnClose(bool btimeOut){
        LOG("called onclose fd: %d pid: %d ", this->GetFd(), this->GetPid());
        close(GetFd());
        m_bclose = true;
        if(btimeOut){
            tcpclientMgr::m_sInst->rpcCallGate((char*)"RegConn", m_pid, 3, NULL, 0);    
        }else{
            tcpclientMgr::m_sInst->rpcCallGate((char*)"RegConn", m_pid, 2, NULL, 0);
        }
    }

    void connObj::OnInit(char* paddr, bool bnet){
        strcpy(m_remoteAddr, paddr);
        unsigned int byteLen = strlen(m_remoteAddr);
        if(bnet){
            tcpclientMgr::m_sInst->rpcCallGate((char*)"RegConn", m_pid, 1, (unsigned char*)m_remoteAddr, byteLen);
        }
    }

    void connObj::chkmem(){
        /*
        for(int i=0; i<1024; i++){
            if (memchk[i] != 0 ){
                assert(false);
            }
            if (memchkAA[i] != 0 ){
                assert(false);
            }
        }
        */
    }
    int connObj::send(unsigned char* buf, size_t size){
        if(m_bclose){
            return 0;
        }
        if( size == 0 && m_sendBufLen == 0){
            return 0;
        }

        //assert( size + m_sendBufLen <= BUFFER_SIZE);
        //chkmem();
        if( buf != NULL ){
            if( size > m_sendBufCap-m_sendBufLen){
                int leftSize = m_sendBufLen-m_sendBufOffset;
                int newSize = size+leftSize;
                char* p = new char[newSize];
                memcpy(p, m_psendBuf+m_sendBufOffset,leftSize);
                memcpy(p+leftSize, buf, size );
                delete[] m_psendBuf;
                m_psendBuf = p;
                m_sendBufOffset = 0;
                m_sendBufCap = newSize;
                m_sendBufLen = newSize;
            }else{
                memcpy(m_psendBuf+m_sendBufLen, buf, size );
            }
        }
        //chkmem();
        m_sendBufLen += size;
        assert(m_sendBufOffset>=0&&m_sendBufOffset<=BUFFER_SIZE);
        assert(m_sendBufLen>=0&&m_sendBufLen<=BUFFER_SIZE);
        size_t s = 0;
        size_t nowSize=0;
        const size_t SEND_SIZE=64*1024;
        int bret=0;
        while(1){
            if(m_sendBufLen-m_sendBufOffset>SEND_SIZE){
                nowSize = SEND_SIZE;
            }else{
                nowSize = m_sendBufLen-m_sendBufOffset;
            }
            s = write(m_fd, m_psendBuf+m_sendBufOffset, nowSize);
            if( s == -1 && errno == EAGAIN ){
                connObjMgr::g_pConnMgr->AddWriteConn(m_fd, this);
                bret = -1;
                break;
            }
            if(s==-1&&errno != EAGAIN ){
                netServer::g_netServer->appendConnClose(m_fd);
                bret = 0;
                break;
            }
            assert(s>0&&s<=nowSize);
            assert(m_sendBufOffset>=0&&m_sendBufOffset<=BUFFER_SIZE);
            assert(m_sendBufLen>=0&&m_sendBufLen<=BUFFER_SIZE);
            m_sendBufOffset += s;
            LOG("connObj::send pid: %d  s: %d m_sendBufOffset: %d m_sendBufLen: %d", m_pid, s, m_sendBufOffset, m_sendBufLen );
            if(m_sendBufOffset >= m_sendBufLen){
                //chkmem();
                if( isNet() ){
                    qpsMgr::g_pQpsMgr->updateQps(3, m_sendBufOffset);
                }
                m_sendBufOffset = 0;
                m_sendBufLen =0;
                //chkmem();
                bret = 0;
                break;
                //return 0;
            }
        }
    }


    int connObj::OnRead(){
        if(m_bclose){
            return 0;
        }
        int bret = 1;
        int ret = _OnRead();
        if( ret ==0 || ret ==1 ){
            bret = 0;
        }

        if(!parseBuff()){
            if( getsec() - m_lastSec >= 5){
                bret = 0;
            }
        }
        resetBuffer();
        return bret;
    }
    void connObj::resetBuffer(){
        if (m_NetOffset<=m_ReadOffset){
            m_NetOffset = 0;
            return;
        }
        memmove(m_NetBuffer,m_NetBuffer+m_ReadOffset,m_NetOffset-m_ReadOffset);
        m_NetOffset = m_NetOffset-m_ReadOffset;
    }

    
    

    int connObj::_OnRead(){
        if(m_bclose){
            return 0;
        }
        ssize_t count;
        count = read (m_fd, m_NetBuffer+m_NetOffset, RECV_BUFFER_SIZE-m_NetOffset);
        if (count == -1)
        {
            //   If errno == EAGAIN, that means we have read all
            //   data. So go back to the main loop. 
            if (errno != EAGAIN){
                netServer::g_netServer->appendConnClose(m_fd);
                return 0;
            }
            m_bChkReadZero = true;
            return 1;
        }
        else if (count == 0&&m_bChkReadZero)
        {
            // End of file. The remote has closed the
            //   connection. 
            netServer::g_netServer->appendConnClose(m_fd);
            return 0;
        }else if (count + m_NetOffset< BUFFER_SIZE){
            m_bChkReadZero = false;
            m_NetOffset += count;
            return 3;
        }else if (count + m_NetOffset== BUFFER_SIZE){
            m_bChkReadZero = false;
            m_NetOffset += count;
            return 4;
        }
    }

    connObj::connObj(int _fd){
        m_fd = _fd;
        m_pid = 0;
        m_lastSec = 0;
        m_NetOffset = 0;
        m_sendBufOffset= 0;
        m_sendBufLen = 0;
        m_bChkReadZero = true;

        m_bclose = false;


        //memset(memchk,0,1024);
        //memset(memchkAA,0,1024);

        memset(m_NetBuffer,0,RECV_BUFFER_SIZE);

        m_psendBuf = new char[BUFFER_SIZE];
        memset(m_psendBuf,0,BUFFER_SIZE);
        m_sendBufCap = BUFFER_SIZE;

        mutexRecv = new pthread_mutex_t;
        pthread_mutex_init( mutexRecv, NULL );
    }

    void connObj::SetPid(int _pid){
        m_pid = _pid;
    }
    int connObj::GetPid(){
        return m_pid;
    }
    int connObj::GetFd(){
        return m_fd;
    }
    void connRpcObj::Update(unsigned int ms){
        OnRead();
        dealSend();
    }


    //for rpcGame
    bool connRpcObj::parseBuff(){
        if(m_NetOffset!=0){
            m_lastSec = getsec();
        }else{
            return false;
        }
        m_ReadOffset = 0;
        unsigned int *psize = NULL;
        while(true){
            if(m_NetOffset-m_ReadOffset<sizeof(int)){
                memmove(m_NetBuffer, m_NetBuffer+m_ReadOffset, m_NetOffset-m_ReadOffset);
                m_NetOffset = m_NetOffset-m_ReadOffset;
                return true;
            }

            psize = (unsigned int*)(m_NetBuffer+m_ReadOffset);
            if(m_NetOffset-sizeof(int)-m_ReadOffset < *psize){
                return true;
            }

            if(*psize>=64*1024){
                netServer::g_netServer->appendConnClose(m_fd);
                LOG("[ERROR] pid: %d msg size: %d exceed 64k", m_pid, *psize);
                return true;
            }
            //assert( rpcObj::chkPt( *pmsgid ) );
            rpcObj* p = new rpcObj();
            p->decodeBuffer(m_NetBuffer+m_ReadOffset);
            p->ToString();
            dealRpcMsg(p);
            delete p;
            m_ReadOffset += *psize +sizeof(int);
        }
        return true;
    }


    void connRpcObj::rpcCallNet(char* target, unsigned long long pid, unsigned int msgid, 
        unsigned char* pbyte, unsigned int byteLen){
        if(!m_pSendCache){
            m_pSendCache = new sendCache;
        }
        int needSize = rpcObj::getRpcSize(target, pid, msgid, pbyte, byteLen);
        if(needSize > m_pSendCache->getLeftSize()){
            m_sendCacheQueue.push( m_pSendCache );
            m_pSendCache = new sendCache;
        }
        assert( needSize < m_pSendCache->getLeftSize() );
        int offset = rpcObj::encodeBuffer(m_pSendCache->getWritePtr(), target, pid, msgid, pbyte, byteLen);
        m_pSendCache->updateOffset(offset);
    }

    int connRpcObj::dealSend(){
        if(m_sendCacheQueue.size() == 0 && m_pSendCache->getOffset() == 0){
            return 0;
        }
        if( m_pSendCache  ){
            m_sendCacheQueue.push( m_pSendCache );
            m_pSendCache = NULL;
        }
        sendCache *p = NULL;
        int ret = 0;
        if(m_sendCacheQueue.size()!=0){
            LOG("deal send size: %d", m_sendCacheQueue.size());
        }
        while(!m_sendCacheQueue.empty()){
            p = m_sendCacheQueue.front();
            m_sendCacheQueue.pop();
            LOG(" send to gate before size: %d limitsize: %d", p->getOffset(), RPC_BUFF_SIZE );
            assert(p->getOffset()<=RPC_BUFF_SIZE);
            if(p->getOffset() == 0){
                delete p;
                continue;
            }
            if(ret != -1 ){
                ret = p->dosend(m_fd);
                if( ret == -1 ){
                    LOG("[ERROR] connRpcObj::dealSend ret fail");
                    OnClose();
                    return -1;
                }else{
                    LOG(" send to net size: %d", p->getOffset() );
                }
            }
            delete p;
        }
        m_pSendCache = NULL;
        return 0;
    }

    void connRpcObj::dealRpcMsg(rpcObj *pobj){
        if( strcmp((const char*)pobj->getTarget(), "TakeProxy") == 0 ){
            LOG("[INFO] connRpcObj TakeProxy, node: %s connedted", pobj->getParam() );
            return;
        }
        rpcGameHandle::m_pInst->process(pobj);
    }

    void connRpcObj::destroy(){
        close(GetFd());
    }

    //connNetObj
    bool connNetObj::IsTimeout(int nowsec){
        if(m_lastSec==0){
            return false;
        }
        if( nowsec - m_lastSec >= 11){
            LOG("nowsec: %d lastsec: %d timeout", nowsec, m_lastSec);
            return true;
        }else{
            return false;
        }
    }

    connNetObj::connNetObj(int _fd):connObj(_fd){
        m_pmsg = new msgObj;
    }
    bool connNetObj::parseBuff(){
        if(m_NetOffset!=0){
            m_lastSec = getsec();
        }else{
            return false;
        }
        m_ReadOffset = 0;
        unsigned int *psize = NULL;
        unsigned int *pmsgid = NULL;
        while(true){
            if(m_NetOffset-m_ReadOffset<sizeof(int)*2){
                memmove(m_NetBuffer, m_NetBuffer+m_ReadOffset, m_NetOffset-m_ReadOffset);
                m_NetOffset = m_NetOffset-m_ReadOffset;
                return true;
            }

            psize = (unsigned int*)(m_NetBuffer+m_ReadOffset);
            pmsgid = (unsigned int*)(m_NetBuffer+m_ReadOffset+sizeof(int));
            if(m_NetOffset-sizeof(int)*2-m_ReadOffset < *psize){
                return true;
            }

            if(*psize>=64*1024){
                netServer::g_netServer->appendConnClose(m_fd);
                LOG("[ERROR] pid: %d msg size: %d msgid: %d exceed 64k", m_pid, *psize, *pmsgid );
                return true;
            }
            assert( rpcObj::chkPt( *pmsgid ) );

            m_pmsg->init(pmsgid, psize, (unsigned char*)(m_NetBuffer+m_ReadOffset+sizeof(int)*2) );

            if( netServer::g_netServer->isNet() ){
                qpsMgr::g_pQpsMgr->updateQps(4, m_pmsg->size());
            }
            dealMsg(m_pmsg);
            m_ReadOffset += *psize +sizeof(int)*2;
        }
    }


    void connNetObj::dealMsg(msgObj *p){
        assert(p);
        int len = 0;
        unsigned int msgid = 0;
        msgid = p->getMsgid();
        if( msgid >=0 && msgid<1999){
            tcpclientMgr::m_sInst->rpcCallGate((char*)"Msg2gate", m_pid, msgid, p->getBodyPtr(), p->getBodylen());
        }else if(msgid >=2000 && msgid <2999){
            tcpclientMgr::m_sInst->rpcCallGame((char*)"Msg2game", m_pid, msgid, p->getBodyPtr(), p->getBodylen());
        }else{
            LOG("[FATAL] pid: %d invalid msgid: %d",m_pid, msgid);
            assert(false);
        }
        p->update();
    }


}
