#include "recvBuff.h"
#include <arpa/inet.h>
#include <string.h>
#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include <unistd.h>
#include <errno.h>
#include <string>

#include "log.h"
#include "net.h"


#define CHUNK 1024

namespace net{

    unsigned int sendCache::g_guid = 0;
    /* msgObj */
    unsigned long long g_uid = 0;
    msgObj::msgObj(){
        m_uid = g_uid++;
    }
    void msgObj::init(unsigned int* msgid, unsigned int* plen, unsigned char* p){
        m_pmsgId = msgid;
        m_pbodyLen = plen;
        m_pBody = p;
    }
    msgObj::msgObj(unsigned int* msgid, unsigned int* plen, unsigned char* p){
        m_uid = g_uid++;
        init(msgid, plen, p);
    }
    msgObj::~msgObj(){
    }

    void msgObj::update(){
        LOG("[NET] msgObj uid: %d update msgid: %d bodylen: %d",m_uid, *m_pmsgId, *m_pbodyLen );
    }

    int msgObj::size(){
        return *m_pbodyLen+8;
    }
    
    rpcObj::~rpcObj(){
        if( m_pBodyLen!=0 &&m_pBodyLen>256) {
            delete[] m_pBuff;
        }
    }
    /* rpcObj */
    rpcObj::rpcObj(){
        init();
    }
    void rpcObj::init(){
        m_pLen = NULL;
        m_pbody = NULL;

        m_pMsgType = 0;

        memset(m_pTarget,0,sizeof(m_pTarget));
        memset(m_pKey,0,sizeof(m_pKey));
        memset(m_pParam,0,sizeof(m_pParam));
        memset(m_pResult,0,sizeof(m_pResult));
        memset(m_buff,0,sizeof(m_buff));

        m_pPid = 0;
        m_pMsgid = 0;
        m_pBuff = NULL; 
        m_pBodyLen = 0;
    }

    void rpcObj::update(){
        if(strcmp((const char*)m_pTarget,"ping")==0){
            LOG("recv ping rpc");
        }else{
            LOG("recv %s rpc", m_pTarget);
        }
    }

    void rpcObj::ToString(){
        char info[1024] = {0};
        sprintf(info,"target: %s key: %s param: %s result: %s msgid: %d bodylen: %d", m_pTarget, m_pKey,m_pParam,m_pResult, m_pMsgid,m_pBodyLen);
        LOG("[rpcobj] tostring: %s", info);

    }

    bool rpcObj::chkPt(int val){
        switch(val) {
            case 0:
            case 503:
            case 98:
            case 99:
            case 100:
            case 101:
            case 102:
            case 103:
            case 104:
            case 105:
            case 106:
            case 107:
            case 108:
            case 109:
            case 110:
            case 111:
            case 113:
            case 114:
            case 115:
            case 116:
            case 117:
            case 118:
            case 119:
            case 120:
            case 121:
            case 122:
            case 123:
            case 124:
            case 125:
            case 126:
            case 127:
            case 128:
            case 129:
            case 130:
            case 131:
            case 132:
            case 133:
            case 134:
            case 135:
            case 140:
            case 141:
            case 142:
            case 150:
            case 151:
            case 152:
            case 153:
            case 154:
            case 155:
            case 158:
            case 160:
            case 161:
            case 162:
            case 163:
            case 164:
            case 170:
            case 171:
            case 173:
            case 180:
            case 181:
            case 182:
            case 183:
            case 184:
            case 190:
            case 191:
            case 192:
            case 193:
            case 2000:
            case 2001:
            case 2002:
            case 2003:
            case 2004:
            case 2005:
            case 210:
            case 211:
            case 212:
            case 213:
            case 214:
            case 215:
            case 216:
            case 217:
            case 218:
            case 219:
            case 220:
            case 221:
            case 222:
            case 223:
            case 251:
            case 252:
            case 253:
            case 254:
            case 255:
            case 256:
            case 257:
            case 259:
            case 260:
            case 280:
            case 281:
            case 282:
            case 283:
            case 284:
            case 285:
            case 286:
            case 287:
            case 288:
            case 289:
            case 290:
            case 291:
            case 292:
            case 293:
            case 294:
            case 258:
            case 300:
            case 311:
            case 312:
            case 313:
            case 314:
            case 320:
            case 321:
            case 322:
            case 323:
            case 400:
            case 401:
            case 402:
            case 403:
            case 404:
            case 413:
            case 406:
            case 407:
            case 408:
            case 409:
            case 410:
            case 412:
            case 415:
            case 414:
            case 416:
            case 417:
            case 418:
            case 419:
            case 420:
            case 421:
            case 422:
            case 423:
            case 424:
            case 426:
            case 427:
            case 500:
            case 501:
            case 502:
            case 520:
            case 522:
            case 540:
            case 542:
            case 600:
            case 601:
            case 602:
            case 603:
            case 604:
            case 1000:
            case 1001:
                return true;
            default:
                return false;
        }
    }

    

    void rpcObj::decodeBuffer(char* p){
        m_pLen = (int*)p;
        m_pbody = (unsigned char*)(p+ sizeof(int));

        //msgtype
        m_pMsgType = *m_pbody;

        assert(m_pMsgType==0);
        //key
        unsigned char *plen = (unsigned char*)(m_pbody+1);
        memcpy(m_pKey, m_pbody+2, *plen);

        //target
        unsigned char *plenTar = (unsigned char*)(m_pbody+2+*plen);
        memcpy(m_pTarget,m_pbody+3+*plen, *plenTar);

        //param
        unsigned char *plenPa = (unsigned char*)(m_pbody+3+*plen + *plenTar);
        memcpy(m_pParam,m_pbody+4+*plen+*plenTar, *plenPa);

        //result
        unsigned char *plenRes = (unsigned char*)(m_pbody+4+*plen + *plenTar + *plenPa);
        memcpy(m_pResult,m_pbody+4+*plen+*plenTar+*plenPa, *plenRes);

        m_pPid = *(unsigned long long*)(m_pbody+5+*plen + *plenTar + *plenPa + *plenRes);
        m_pMsgid= *(unsigned int*)(m_pbody+5+*plen + *plenTar + *plenPa +*plenRes+ sizeof(unsigned long long));

        if(netServer::g_netServer->isNet()){
            //assert( chkPt( m_pMsgid ) );
        }

        m_pBodyLen = *(unsigned int*)(m_pbody+5+*plen + *plenTar + *plenPa + *plenRes + sizeof(unsigned long long) + sizeof(unsigned int));
        if(m_pBodyLen==0){
            return;
        }
        unsigned char* ptmp= (unsigned char*)(m_pbody+5+*plen + *plenTar + *plenPa + *plenRes + sizeof(unsigned long long) + sizeof(unsigned int) + sizeof(unsigned int) );
        if(m_pBodyLen>256){
            m_pBuff = (unsigned char*)new char[m_pBodyLen];
            memcpy(m_pBuff, ptmp, m_pBodyLen );
        }else{
            memcpy(m_buff,ptmp, m_pBodyLen );
            m_pBuff = m_buff;
        }
    }


    unsigned int rpcObj::getRpcSize(char* target, unsigned long long pid, unsigned int msgid, unsigned char* pbyte, unsigned int byteLen){
        const char* param = (const char*)netServer::g_netServer->getName();
        const char* result = (const char*)"defres";        
        const char* key = (const char*)"";        

        int size = 0;
        size += sizeof(char); //msgtp
        size += sizeof(char); //key len
        size += strlen(key); //key content

        //target
        size += sizeof(char);
        size += strlen(target);

        //param
        size += sizeof(char);
        size += strlen(param);

        //res
        size += sizeof(char);
        size += strlen(result);

        //pid
        size += sizeof(pid);
        //msgid
        size += sizeof(msgid);
        //bodylen
        size += sizeof(int);
        //body
        size += byteLen;
        return size;
    }

    unsigned int rpcObj::encodeBuffer(unsigned char* p,char* target, unsigned long long pid, unsigned int msgid, unsigned char* pbyte, unsigned int byteLen){
        unsigned char *pSize = (unsigned char* )p;

        const char* param = (const char*)netServer::g_netServer->getName();
        const char* result = (const char*)"defres";        
        const char* key = (const char*)"";        

        LOG("rpcObj::encodeBuffer param: %s target: %s pid: %d msgid: %d", param, target, pid, msgid );
        unsigned char msgtp = 0;
        //msgtype
        memcpy(p+sizeof(int), &msgtp, 1);

        //key
        unsigned char len = strlen(key);
        memcpy( p+sizeof(int)+1, &len, 1 );
        memcpy( p+sizeof(int)+2, key, len );
        //ignore cpy keyval, for it's empty

        //target
        unsigned char len1 = strlen(target);
        memcpy( p+sizeof(int)+2+len, &len1, 1 );
        memcpy( p+sizeof(int)+3+len, target, len1 );

        //param
        unsigned char lenpa = strlen(param);
        memcpy( p+sizeof(int)+3+len+len1, &lenpa, 1 );   
        memcpy( p+sizeof(int)+4+len+len1, param, lenpa );  

        //res
        unsigned char lenres = strlen(result);
        memcpy( p+sizeof(int)+4+len1+len+lenpa, &lenres, 1 );   
        memcpy( p+sizeof(int)+5+len1+len+lenpa, result, lenres );  

        //pid
        memcpy( p+sizeof(int)+5+len1+len+lenpa+lenres, &pid, sizeof(pid) );  
        //msgid        
        memcpy( p+sizeof(int)+5+len1+len+lenpa+lenres+sizeof(pid), &msgid, sizeof(msgid) );  

        //bodylen
        memcpy( p+sizeof(int)+5+len1+len+lenpa+lenres+sizeof(pid)+sizeof(msgid), &byteLen, sizeof(byteLen) ); 
        //body
        memcpy( p+sizeof(int)+5+len1+len+lenpa+lenres+sizeof(pid)+sizeof(msgid)+sizeof(byteLen), pbyte, byteLen ); 

        //unsigned int size = sizeof(int)+5+len1+len+lenpa+lenres+sizeof(pid)+sizeof(msgid)+sizeof(byteLen)+byteLen;
        unsigned int size = getRpcSize(target, pid, msgid, pbyte, byteLen );
        memcpy(pSize, &size, sizeof(size));
        return size + sizeof(size);
    }


    sendCache::sendCache(){
        memset(m_mem,0,RPC_BUFF_SIZE);
        m_offset = 0;
        m_uid = g_guid++;
        LOG("new sendCache uid: %d", m_uid );
    }

    sendCache::~sendCache(){
        LOG("delete sendCache uid: %d", m_uid );
    }

    void sendCache::init(){
        memset(m_mem,0,RPC_BUFF_SIZE);
        m_offset = 0;
    }
    int sendCache::dosend(int fd){
        unsigned char* pbuf = this->getPtr();
        size_t size = this->getOffset();
        size_t s = 0;
        size_t nowSize=0;
        const size_t SEND_SIZE=64*1024;
        int offset = 0;
        int bret = 0;

        bool eagain = false;

        while(1){
            if(size-offset>SEND_SIZE){
                nowSize = SEND_SIZE;
            }else{
                nowSize = size-offset;
            }
            if( nowSize <= 0 ){
                bret = 0;
                break;
            }
            s = write(fd, pbuf+offset, nowSize);
            if( s == -1 && errno == EAGAIN ){
                usleep(10);
                eagain = true;
                continue;
            }
            if(s==-1&&errno != EAGAIN ){
                //tcpclientMgr::m_sInst->DelConn(m_sock);
                return -1;
            }
            if( eagain ){
                LOG("send eagain size: %d", s );
            }
            offset += s;
        }
        this->updateOffset(offset);
        return bret;        
    }
}
