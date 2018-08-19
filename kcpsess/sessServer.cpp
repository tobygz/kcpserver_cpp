#include "sessServer.h"

#include <iostream>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

#include "../recvBuff.h"
#include "../log.h"
#include "../rpchandle.h"
#include "../qps.h"
#include "../net.h"


#define IKCP_OVERHEAD 24
#define MAXEVENT 64
#define KCP_CONN_TIMEOUT_MS 1000*20 
#define KCP_CONN_TIMEOUT_FREQ_MS 1000*2 //2s
#define KCP_UDPCONN_SIZE 8192 //2s

using namespace net;

//#define NI_MAXHOST  32
//#define NI_MAXSERV  16

void printBytes(unsigned char *val, size_t size, char *str) {
    const unsigned char* p = val;
    char info[4096] = "[";
    int i = 0;
    for (i = 0; i<size; i++) {
        if (i == 0) {
            sprintf(info, "%s%d", info, p[i]);
        }
        else {
            sprintf(info, "%s,%d", info, p[i]);
        }
    }
    strcat(info, "]");  
    sprintf(info,"%s len: %ld", info, size );

    LOG("[%s] bin: %s", str, info);
}


KCPServer* KCPServer::m_sInst = NULL; //new KCPServer;
long KCPServer::g_sess_id= 0;

int setnonblocking(int sockfd)
{
    if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK) == -1) 
    {   
        return -1; 
    }   
    return 0;
}

static void add_event(int epollfd,int fd,int state)
{
    struct epoll_event ev; 
    memset(&ev,0, sizeof(ev));
    ev.events = state;
    ev.data.fd = fd; 
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev);
} 
static void delete_event(int epollfd,int fd,int state)
{
    struct epoll_event ev; 
    memset(&ev,0, sizeof(ev));
    ev.events = state;
    ev.data.fd = fd; 
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,&ev);
}


UDPConn::UDPConn(){
    mutex = new pthread_mutex_t;
    pthread_mutex_init( mutex, NULL );
    mutex_kcp = new pthread_mutex_t;
    pthread_mutex_init( mutex_kcp, NULL );
}

void UDPConn::init(int fd, int epfd, int id, unsigned char* pbuff, int len){
    m_fd = fd;
    m_offset = 0;
    m_epollFd = epfd;
    m_id = id;

    m_conv = *(int*)pbuff;
    m_bornTick = net::currentMs();

    m_bind = false;
    m_bread = true;

    pthread_mutex_lock(mutex_kcp);
    m_kcp = ikcp_create( m_conv, this);
    ikcp_wndsize(m_kcp, 128, 128);
    ikcp_nodelay(m_kcp, 1,10,2,1);
    ikcp_setmtu(m_kcp, 1400);
    m_kcp->stream = 1;
    m_kcp->output = out_wrapper;

    if( len != 0 ){
        ikcp_input(m_kcp, (const char*)pbuff, len);
    }
    LOG("udpconn inited fd: %d id: %d len: %d conv: %d m_kcp: %p",fd, id, len, m_conv, m_kcp);
    pthread_mutex_unlock(mutex_kcp);
}

void UDPConn::Bind(){
    m_bind = true;
}

void UDPConn::Close(){
    close(m_fd);
    
    pthread_mutex_lock(mutex_kcp);
    if(m_kcp){
        ikcp_release(m_kcp);
        m_kcp = NULL;
    }
    pthread_mutex_unlock(mutex_kcp);

    LOG("udpconn closed sessid: %d fd: %d ",m_id, m_fd);
}
//called by mainloop
void UDPConn::Update(unsigned int ms){
    pthread_mutex_lock(mutex_kcp);
    if(m_kcp){
        ikcp_update(m_kcp, ms);
    }
    pthread_mutex_unlock(mutex_kcp);
}
//called by epthread
bool UDPConn::OnRead(){
    int nread = read(m_fd, m_buf, READ_BUFF_SIZE);
    if(nread<=0 && errno!= EAGAIN){
        return false;
    }
    setRead();
    pthread_mutex_lock(mutex_kcp);
    ikcp_input(m_kcp, (char *) (m_buf), nread);
    pthread_mutex_unlock(mutex_kcp);
    return true;
}

bool UDPConn::OnCheckTimeout(unsigned int ms){
    unsigned int bornTick = 0;
    bool bind = false;
    pthread_mutex_lock(mutex);
    bornTick = m_bornTick;
    bind = m_bind;
    pthread_mutex_unlock(mutex);
    if(bind){
        return false;
    }

    if(bind == false && ms-bornTick > KCP_CONN_TIMEOUT_MS ) {
        return true;
    }
    return false;
}

int UDPConn::OnDealMsg(unsigned int ms, msgObj* pmsg){
    if(m_bread==false){
        return 0;
    }
    m_bread = false;
    /*
    pthread_mutex_lock(mutex);
    m_lastTick = ms;
    pthread_mutex_unlock(mutex);
    */

    pthread_mutex_lock(mutex_kcp);
    int psz = ikcp_peeksize(m_kcp);
    pthread_mutex_unlock(mutex_kcp);

    if (psz <= 0) {
        return -1;
    }

    if(psz>BUFF_CACHE_SIZE){
        LOG("invalied OnDealMsg for psz: %d\n", psz);
        return -2;
    }

    pthread_mutex_lock(mutex_kcp);
    size_t nread = ikcp_recv(m_kcp, (char*)m_cacheBuf+m_offset, int(BUFF_CACHE_SIZE)-m_offset);
    pthread_mutex_unlock(mutex_kcp);

    if(nread<0){
        LOG("ikcp_recv nread: %d\n", nread);
        return nread;
    }

    unsigned int *bodylen = (unsigned int*)(m_cacheBuf+m_offset);
    if(*bodylen>nread){
        return -3;
    }
    unsigned int *pmsgid = (unsigned int*)(m_cacheBuf+m_offset+4);
    unsigned long long ppid = *(unsigned long long*)(m_cacheBuf+m_offset+8);
    //int roomid = (ppid&0xffffffff00000000) >> 32;
    int pid = ppid&0x00000000ffffffff;
    pmsg->init(pmsgid, bodylen, (unsigned char*)(m_cacheBuf+m_offset+16));

    rpcGameHandle::m_pInst->process(pmsg, pid, m_id);

    qpsMgr::g_pQpsMgr->updateQps(4, *bodylen);

    size_t len = 16 + *bodylen;
    memmove(m_cacheBuf, m_cacheBuf+len, nread-len);
    m_offset += nread - len;
    assert(m_offset == 0 );

    return 0;
}

size_t UDPConn::Write(const char *buf, size_t sz) {
    ssize_t n = 0;
    pthread_mutex_lock(mutex_kcp);
    if(m_kcp){
        n = ikcp_send(m_kcp, const_cast<char *>(buf), int(sz));
    }
    pthread_mutex_unlock(mutex_kcp);
    if (n == 0) {
        return sz;
    } else return n;
}

int UDPConn::out_wrapper(const char *buf, int len, struct IKCPCB *, void *user) {    
    UDPConn *pcon = static_cast<UDPConn *>(user);
    pcon->output(buf, static_cast<size_t>(len));
    return 0;
}

ssize_t UDPConn::output(const void *buffer, size_t length) {
    ssize_t n = send(m_fd, buffer, length, 0);
    return n;
}


//lock outside
void KCPServer::sendMsg(int sessid, unsigned char* pbuff, int size){
    if(sessid == 0 ){
        return;
    }
    map<int,int>::iterator it = m_mapSessFd.find(sessid);
    if(it==m_mapSessFd.end()){
        LOG("[ERROR] sendMsg failed, sessid: %d size: %d", sessid, size);
        return;
    }  
    map<int,UDPConn*>::iterator it1 = m_mapConn.find(it->second);
    if(it1==m_mapConn.end()){
        LOG("[ERROR] sendMsg failed, sessid: %d fd: %d size: %d", sessid, it->second, size);
        return;
    } 
    UDPConn *p = it1->second;
    p->Write( (const char*)pbuff, size );
}

//lock outside
void KCPServer::rawCloseConn(int sessid){
    UDPConn *pconn = rawGetConn(sessid);
    if(!pconn){
        return;
    }
    map<int,UDPConn*>::iterator it = m_mapConn.find(pconn->getfd());
    if(it==m_mapConn.end()){
        return;
    }
    UDPConn *p = it->second;
    m_mapConn.erase(it->first);
    m_mapSessFd.erase( p->getid() );
    LOG("delConn sessid: %d fd: %d all: %s",p->getid(), p->getfd(), getAllSessid().c_str());
    p->Close();
    pushUDPConn(p);
}

void KCPServer::BindConn(int sessid){
   UDPConn *pconn = rawGetConn(sessid); 
   if(!pconn){
       return;
   }
   pconn->Bind();
}

void KCPServer::closeConn(int sessid){
    pthread_mutex_lock(mutex);
    UDPConn *pconn = rawGetConn(sessid);
    if(!pconn){
        pthread_mutex_unlock(mutex);
        return;
    }
    pthread_mutex_unlock(mutex);
    delConn(pconn->getfd());
}

string KCPServer::getAllSessid(){
    stringstream ss;
    //pthread_mutex_lock(mutex);
    for(map<int,int>::iterator it=m_mapSessFd.begin(); it!=m_mapSessFd.end(); it++){
        ss << it->first;
        ss << " ";
    }
    //pthread_mutex_unlock(mutex);
    return ss.str();
}

void KCPServer::delConn(int fd){
    pthread_mutex_lock(mutex);
    map<int,UDPConn*>::iterator it = m_mapConn.find(fd);
    if(it==m_mapConn.end()){
        pthread_mutex_unlock(mutex);
        return;
    }
    UDPConn *p = it->second;
    m_mapConn.erase(fd);
    m_mapSessFd.erase( p->getid() );
    LOG("delConn sessid: %d clifd: %d all: %s", p->getid(), p->getfd(), getAllSessid().c_str());
    p->Close();
    pushUDPConn(p);
    pthread_mutex_unlock(mutex);
}

UDPConn* KCPServer::createConn(int clifd, unsigned char* buf, int len){
    UDPConn* pcon = NULL;
    pthread_mutex_lock(mutex);
    g_sess_id++;
    pcon = popUDPConn(); 
    pcon->init(clifd, m_epollFd, g_sess_id, buf, len);
    m_mapConn[clifd] = pcon;
    m_mapSessFd[g_sess_id] = clifd;
    LOG("createConn sessid: %d clifd: %d len: %d",pcon->getid(), clifd, len);
    setnonblocking(clifd);
    if( len != 0 ){
        pcon->setRead();
    }
    pthread_mutex_unlock(mutex);
    return pcon;
}


void KCPServer::processMsg(int clifd){
    pthread_mutex_lock(mutex);
    map<int,UDPConn*>::iterator it = m_mapConn.find(clifd);
    if(it==m_mapConn.end()){
        LOG("processMsg failed clifd not found", clifd);
        pthread_mutex_unlock(mutex);
        return;
    }
    UDPConn* p = it->second;
    assert(p);
    p->OnRead();
    pthread_mutex_unlock(mutex);
}

void KCPServer::acceptConn()
{
    unsigned char buf[4096] = {0};
    while(true){
        struct sockaddr_storage  client_addr;
        bzero(&client_addr, sizeof(client_addr));
        socklen_t addr_size = sizeof(client_addr);
        int rret=0;
        while(true){
            rret = recvfrom(m_servFd, buf,4096, 0, (struct sockaddr *)&client_addr, &addr_size);
            if( rret == 0 && errno == 0 ){
                continue;
            }
            LOG("format recv from fd: %d ret: %d errno: %d", m_servFd, rret, errno );

            if( rret < 0){
                LOG("invalied recv from client_addr fd: %d", m_servFd);
                return;
            }

            if( rret >0){
                break;
            }
        }

        if( rret < IKCP_OVERHEAD) {
            LOG("invalied format recv from addr len: %d ", rret);
            return;
        }

        char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
        getnameinfo((struct sockaddr *)&client_addr, addr_size, hbuf, sizeof(hbuf), \
                sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);

        struct sockaddr_in my_addr ;
        int clifd=socket(PF_INET, SOCK_DGRAM, 0);

        /*设置socket属性，端口可以重用*/
        int opt=SO_REUSEADDR;
        setsockopt(clifd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

        bzero(&my_addr, sizeof(my_addr));
        my_addr.sin_family = PF_INET;
        my_addr.sin_port = htons(m_listenPort);
        my_addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(clifd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1) 
        {
            perror("bind");
            exit(1);
        } 
        else
        {
            //LOG("IP and port bind success ");
        }
        if(clifd==-1){
            perror("fatal eror here");
            exit(1);
            return ;
        }
        connect(clifd,(struct sockaddr*)&client_addr,sizeof(struct sockaddr_in));
        add_event(m_epollFd,clifd,EPOLLIN);

        LOG("recvfrom client [%s:%s] fd: %d len: %d", hbuf, sbuf, clifd, rret );
        createConn( clifd, buf, rret );

    }
}


unsigned long int KCPServer::Listen(const int lport){

    m_listenPort = lport;

    m_servFd = socket(PF_INET, SOCK_DGRAM, 0);

    m_pServaddr = new sockaddr_in;
    bzero(m_pServaddr, sizeof(sockaddr_in));
    m_pServaddr->sin_family = PF_INET;
    m_pServaddr->sin_addr.s_addr = INADDR_ANY;
    m_pServaddr->sin_port = htons(m_listenPort);

    int opt=SO_REUSEADDR;
    setsockopt(m_servFd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    setnonblocking(m_servFd);

    if(-1 == bind(m_servFd, (struct sockaddr *)m_pServaddr, sizeof(sockaddr))){
        LOG("error in bind errno: %d\n", errno);
        usleep(10000);
        exit(1);
        return -1;
    }

    m_epollFd = epoll_create(64);
    struct epoll_event ev;    
    ev.events = EPOLLIN | EPOLLET;
    //ev.events = EPOLLIN ;
    ev.data.fd = m_servFd;
    if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_servFd, &ev) < 0) 
    {
        LOG("epoll set insertion error: fd=%d\n", m_servFd);
        return -1;
    }
    else
    {
        LOG("listen socket added in epoll success: %d sfd: %d epfd: %d", lport, m_servFd, m_epollFd);
    }   

    pthread_t id;
    int ret;
    ret=pthread_create(&id,NULL, &KCPServer::epThread , KCPServer::m_sInst);
    if(ret!=0){
        LOG("Create pthread error!\n");
        usleep(100);
        exit (1);
    }

    return id;
}

void* KCPServer::epThread(void* param){
    KCPServer *pthis = (KCPServer*)param;
    struct epoll_event events[MAXEVENT];

    int nfds=0,n=0;
    unsigned int ms = net::currentMs();
    while (1) 
    {

        do{    
            nfds = epoll_wait(pthis->getEpfd(), events, MAXEVENT, -1);
        }while(nfds<0&&errno == EINTR);

        for (n = 0; n < nfds; ++n)
        {
            if (events[n].data.fd == pthis->getServFd()) 
            {   
                pthis->acceptConn();
            }   
            else
            {   
                pthis->processMsg(events[n].data.fd);
            }
        }

        ms = net::currentMs();
        pthis->OnCheckTimeout(ms);
    }
    close(pthis->getEpfd());
    return NULL;

}

void KCPServer::Destroy(){
    for(map<int,UDPConn*>::iterator it=m_mapConn.begin(); it!=m_mapConn.end(); it++){
        it->second->Close();
    }
    close(m_servFd);
    close(m_epollFd);
}
KCPServer::KCPServer(){
    mutex = new pthread_mutex_t;
    pthread_mutex_init( mutex, NULL );

    m_lastTick = 0;
    m_pMsg = new msgObj();
    initPool();
}


void KCPServer::initPool(){
    if( netServer::g_netServer->isNet() ){
        return;
    }
    for(int i=0; i<KCP_UDPCONN_SIZE; i++){
        UDPConn *p = new UDPConn;
        m_pool.push(p);
    }
}

void KCPServer::pushUDPConn(UDPConn* p){
    m_pool.push(p);
}
UDPConn* KCPServer::popUDPConn(){
    UDPConn *p = NULL;
    if(m_pool.empty()){
        p = new UDPConn;
        LOG("!!!!!!!!!! unexpected error, pool UDPConn* was recreated");
    }else{
        p = m_pool.front();
        m_pool.pop();
    }
    return p;
}

UDPConn* KCPServer::rawGetConn(int sessid){

    map<int, int>::iterator it0 = m_mapSessFd.find(sessid);
    if(it0 == m_mapSessFd.end()){
        return NULL;
    }

    map<int,UDPConn*>::iterator it = m_mapConn.find(it0->second);
    if( it == m_mapConn.end()){
        return NULL;
    }
    return it->second;
}


int KCPServer::getCount(){
    int ret = 0;
    pthread_mutex_lock(mutex);
    ret = m_mapConn.size();
    pthread_mutex_unlock(mutex);
    return ret;
}

void KCPServer::OnCheckTimeout(unsigned int ms){
    queue<int> delQue;
    pthread_mutex_lock(mutex);
    if (m_lastTick == 0 ){
        m_lastTick = ms;
    }else if(ms-m_lastTick>KCP_CONN_TIMEOUT_FREQ_MS){
        //do check timeout
        for(map<int,UDPConn*>::iterator it=m_mapConn.begin(); it!=m_mapConn.end(); it++){
            if (it->second->OnCheckTimeout(ms)){
                delQue.push(it->first);
            }
        }
        m_lastTick = ms;
    }
    pthread_mutex_unlock(mutex);

    while(!delQue.empty()){
        int fd = delQue.front();
        LOG("close timeout kcp conn fd: %d", fd);
        KCPServer::m_sInst->delConn( fd );
        delQue.pop();
    }
}

//called  mainloop
void KCPServer::Update(unsigned int ms){
    //when ms ready

    pthread_mutex_lock(mutex);
    for(map<int,UDPConn*>::iterator it=m_mapConn.begin(); it!=m_mapConn.end(); it++){
        it->second->Update(ms);
        it->second->OnDealMsg(ms,m_pMsg);
    }

    pthread_mutex_unlock(mutex);
}

