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

#include "../recvBuff.h"
#include "../log.h"
#include "../rpchandle.h"


#define IKCP_OVERHEAD 24
#define MAXEVENT 64

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
    sprintf(info,"%s len: %d", info, size );

    LOG("[%s] bin: %s", str, info);
}


KCPServer* KCPServer::m_sInst = new KCPServer;
long KCPServer::g_sess_id= 0;
/*
   setnonblocking – 设置句柄为非阻塞方式
   */
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
    ev.events = state;
    ev.data.fd = fd; 
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev);
} 
static void delete_event(int epollfd,int fd,int state)
{
    struct epoll_event ev; 
    ev.events = state;
    ev.data.fd = fd; 
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,&ev);
}
static void do_write(int epollfd,int fd,char *buf)
{
    int nwrite;
    nwrite = write(fd,buf,strlen(buf));
    if (nwrite == -1) 
    {   
        perror("write error:");
        close(fd);
        delete_event(epollfd,fd,EPOLLOUT);
    }   
}

UDPConn::UDPConn(int fd, int epfd, int pid, char* pbuff, int len){
    m_fd = fd;
    m_offset = 0;
    m_epollFd = epfd;
    m_bRead = false;
    m_pid = pid;
    m_conv = *(int*)pbuff;

    m_kcp = ikcp_create( m_conv, this);
    ikcp_wndsize(m_kcp, 128, 128);
    ikcp_nodelay(m_kcp, 1,10,2,1);
    ikcp_setmtu(m_kcp, 1400);
    m_kcp->stream = 1;

    m_kcp->output = out_wrapper;

    LOG("==>udpconn init len: %d \n", len);
    if( len != 0 ){
        ikcp_input(m_kcp, pbuff, len);
    }
}

void UDPConn::Close(){
    close(m_fd);
    printf("udpconn closed pid: %d\n", m_pid);
    //delete_event(m_epollFd, )
}
void UDPConn::Update(unsigned int ms){
    ikcp_update(m_kcp, ms);
}
void UDPConn::OnRead(){
    int nread = read(m_fd, m_buf, READ_BUFF_SIZE);
    if(nread<=0 && errno!= EAGAIN){
        //KCPServer::m_sInst->delConn(m_fd);
        return;
    }
    //int nsend = send(m_fd, m_buf, nread, 0);
    LOG("UDPConn::OnRead read size: %d errno: %d fd: %d", nread, errno, m_fd);
    printBytes(m_buf, nread, (char*)"[UDPConn::OnRead]");
    ikcp_input(m_kcp, (char *) (m_buf), nread);
    KCPServer::m_sInst->markRead(this);
}

int UDPConn::OnDealMsg(){
    if(!m_bRead){
        return 0;
    }

    int psz = ikcp_peeksize(m_kcp);
    if (psz <= 0) {
        return -1;
    }

    if(psz>BUFF_CACHE_SIZE){
        printf("invalied OnDealMsg for psz: %d\n", psz);
        return -2;
    }

    size_t nread = ikcp_recv(m_kcp, (char*)m_cacheBuf+m_offset, int(BUFF_CACHE_SIZE)-m_offset);
    if(nread<0){
        printf("ikcp_recv nread: %d\n", nread);
        return nread;
    }

    /*
       char info[512] = {0};
       memcpy(info, m_cacheBuf, nread);
       Write(info, nread);
       */

    unsigned int *bodylen = (unsigned int*)(m_cacheBuf+m_offset);
    if(*bodylen>nread){
        return -3;
    }
    unsigned int *pmsgid = (unsigned int*)(m_cacheBuf+m_offset+4);
    unsigned long long ppid = *(unsigned long long*)(m_cacheBuf+m_offset+8);
    int roomid = (ppid&0xffffffff00000000) >> 32;
    int pid = ppid&0x00000000ffffffff;
    msgObj *pmsg = new msgObj(pmsgid, bodylen, (unsigned char*)(m_cacheBuf+m_offset+16));
    rpcGameHandle::m_pInst->process(pmsg, pid, m_pid);
    delete pmsg;

    size_t len = 16 + *bodylen;
    memmove(m_cacheBuf, m_cacheBuf+len, nread-len);
    m_offset += nread - len;
    assert(m_offset == 0 );
    LOG("ikcp_recv msgid: %d pid: %d roomid: %d bodylen: %d nread: %d m_offset: %d len: %d", *pmsgid, pid, roomid, *bodylen, nread, m_offset, len );
    printBytes(m_cacheBuf+m_offset+16, *bodylen, (char*)"[ikcp_recv]");

    return 0;
}

size_t UDPConn::Write(const char *buf, size_t sz) {
    ssize_t n = ikcp_send(m_kcp, const_cast<char *>(buf), int(sz));
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

void KCPServer::markRead(UDPConn* pcon){    
    if(pcon==NULL){
        return;
    }
    pcon->markRead();
    m_readMap[pcon->getpid()] = true;
}

//lock outside
void KCPServer::sendMsg(int sessid, unsigned char* pbuff, int size){
    map<int,int>::iterator it = m_mapSessFd.find(sessid);
    if(it==m_mapSessFd.end()){
        LOG("[ERROR] sendMsg failed, sessid: %d size: %d", sessid, size);
        return;
    }  
    map<int,UDPConn*>::iterator it1 = m_mapConn.find(it->second);
    if(it1==m_mapConn.end()){
        LOG("[ERROR] sendMsg failed, sessid: %d fd: %d size: %d", sessid, it->second, size);
        pthread_mutex_unlock(mutex);
        return;
    } 
    it1->second->Write( (const char*)pbuff, size );
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

void KCPServer::delConn(int fd){
    pthread_mutex_lock(mutex);
    map<int,UDPConn*>::iterator it = m_mapConn.find(fd);
    if(it==m_mapConn.end()){
        pthread_mutex_unlock(mutex);
        return;
    }
    UDPConn *p = it->second;
    p->Close();
    m_mapConn.erase(it);
    m_mapSessFd.erase( p->getpid() );
    LOG("delConn clifd: %d pid: %d len: %d\n", p->getfd(), p->getpid());
    delete p;
    pthread_mutex_unlock(mutex);
}

UDPConn* KCPServer::createConn(int clifd, char* buf, int len){
    UDPConn* pcon = NULL;
    pthread_mutex_lock(mutex);
    g_sess_id++;
    pcon = new UDPConn(clifd, m_epollFd, g_sess_id, buf, len);
    m_mapConn[clifd] = pcon;
    m_mapSessFd[g_sess_id] = clifd;
    LOG("createConn clifd: %d pid: %d len: %d\n", clifd, pcon->getpid(), len);
    setnonblocking(clifd);
    if( len != 0 ){
        markRead(pcon);
    }
    pthread_mutex_unlock(mutex);
    return pcon;
}


void KCPServer::processMsg(int clifd){
    pthread_mutex_lock(mutex);
    map<int,UDPConn*>::iterator it = m_mapConn.find(clifd);
    if(it==m_mapConn.end()){
        LOG("processMsg failed clifd not found\n", clifd);
        pthread_mutex_unlock(mutex);
        return;
    }
    it->second->OnRead();
    pthread_mutex_unlock(mutex);
}

void KCPServer::acceptConn()
{
    while(true){
        struct sockaddr_storage  client_addr;
        bzero(&client_addr, sizeof(client_addr));
        socklen_t addr_size = sizeof(client_addr);
        char buf[1024] = {0};
        int ret = 0, rret=0;
        while(true){
            rret = recvfrom(m_servFd, buf,1024, 0, (struct sockaddr *)&client_addr, &addr_size);
            if( rret == 0 && errno == 0 ){
                continue;
            }
            printf("format recv from fd: %d ret: %d errno: %d\n", m_servFd, rret, errno );

            if( rret < 0){
                printf("invalied recv from client_addr fd: %d\n", m_servFd);
                //disconn
                return;
            }
            //check(ret > 0, "recvfrom error");
            //

            if( rret >0){
                break;
            }
        }

        if( rret < IKCP_OVERHEAD) {
            printf("invalied format recv from addr: client_addr 11\n");
            //disconn
            return;
        }

        char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
        ret = getnameinfo((struct sockaddr *)&client_addr, addr_size, hbuf, sizeof(hbuf), \
                sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);

        struct sockaddr_in my_addr, their_addr;
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
            printf("IP and port bind success \n");
        }
        if(clifd==-1){
            perror("fatal eror here");
            exit(1);
            return ;
        }
        connect(clifd,(struct sockaddr*)&client_addr,sizeof(struct sockaddr_in));
        add_event(m_epollFd,clifd,EPOLLIN);

        //check(ret == 0, "getnameinfo");

        LOG("recvfrom client [%s:%s] fd: %d len: %d\n", hbuf, sbuf, clifd, rret );
        //write(clifd, buf, rret);

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
        printf("error in bind errno: %d\n", errno);
        return -1;
    }

    m_epollFd = epoll_create(64);
    struct epoll_event ev;    
    ev.events = EPOLLIN | EPOLLET;
    //ev.events = EPOLLIN ;
    ev.data.fd = m_servFd;
    if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_servFd, &ev) < 0) 
    {
        printf("epoll set insertion error: fd=%d\n", m_servFd);
        return -1;
    }
    else
    {
        printf("listen socket added in epoll success :%d sfd: %d epfd: %d\n", lport, m_servFd, m_epollFd);
    }   

    pthread_t id;
    int i,ret;
    ret=pthread_create(&id,NULL, &KCPServer::epThread , KCPServer::m_sInst);
    if(ret!=0){
        printf ("Create pthread error!\n");
        exit (1);
    }

    return id;
}

void* KCPServer::epThread(void* param){
    KCPServer *pthis = (KCPServer*)param;
    struct epoll_event events[MAXEVENT];

    int nfds=0,n=0;
    char info[512] = {0};
    while (1) 
    {

        do{    
            nfds = epoll_wait(pthis->getEpfd(), events, MAXEVENT, -1);
        }while(nfds<0&&errno == EINTR);

        info[0] = 0;
        for (n = 0; n < nfds; ++n)
        {
            //sprintf(info, "%s; idx: %d -> fd: %d", info, n, events[n].data.fd );
            if (events[n].data.fd == pthis->getServFd()) 
            {   
                pthis->acceptConn();
            }   
            else
            {   
                pthis->processMsg(events[n].data.fd);
            }
        }
        //printf("epoll_wait ret: %s listenFd: %d\n", info, pthis->m_servFd);
    }
    close(pthis->getEpfd());

}

void KCPServer::Destroy(){
    for(map<int,UDPConn*>::iterator it=m_mapConn.begin(); it!=m_mapConn.end(); it++){
        it->second->Close();
    }
    close(m_servFd);
    close(m_epollFd);
}

static inline void itimeofday(long *sec, long *usec)
{
#if defined(__unix)
    struct timeval time;
    gettimeofday(&time, NULL);
    if (sec) *sec = time.tv_sec;
    if (usec) *usec = time.tv_usec;
#else
    static long mode = 0, addsec = 0;
    BOOL retval;
    static IINT64 freq = 1;
    IINT64 qpc;
    if (mode == 0) {
        retval = QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        freq = (freq == 0)? 1 : freq;
        retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
        addsec = (long)time(NULL);
        addsec = addsec - (long)((qpc / freq) & 0x7fffffff);
        mode = 1;
    }
    retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
    retval = retval * 2;
    if (sec) *sec = (long)(qpc / freq) + addsec;
    if (usec) *usec = (long)((qpc % freq) * 1000000 / freq);
#endif
}
static inline IINT64 iclock64(void)
{
    long s, u;
    IINT64 value;
    itimeofday(&s, &u);
    value = ((IINT64)s) * 1000 + (u / 1000);
    return value;
}

static inline IUINT32 iclock()
{
    return (IUINT32)(iclock64() & 0xfffffffful);
}

KCPServer::KCPServer(){
    mutex = new pthread_mutex_t;
    pthread_mutex_init( mutex, NULL );
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


//called  mainloop
void KCPServer::Update(unsigned int ms){

    ms = iclock();
    queue<int> delQue;
    //when ms ready
    pthread_mutex_lock(mutex);
    for(map<int,UDPConn*>::iterator it=m_mapConn.begin(); it!=m_mapConn.end(); it++){
        it->second->Update(ms);
    }

    //every called
    for(map<int,bool>::iterator it=m_readMap.begin(); it!= m_readMap.end(); it++ ){
        UDPConn *p = rawGetConn(it->first);
        if(!p){ continue ;}
        if( p->OnDealMsg() < 0 ){
            //delQue.push(p->getfd());
        }
    }
    m_readMap.clear();
    pthread_mutex_unlock(mutex);
    while(delQue.empty()==false){
        KCPServer::m_sInst->delConn(delQue.front());
        delQue.pop();
    }
}

