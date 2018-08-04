#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>

#include "connmgr.h"
#include "tcpclient.h"
#include "qps.h"
#include "log.h"

#define MAXEVENTS 1024

namespace net{

    netServer* netServer::g_netServer = new netServer;
    netServer* netServer::g_netRpcServer = new netServer;
    bool netServer::g_run = true;

    netServer::netServer(){
        mutex = new pthread_mutex_t;
        pthread_mutex_init( mutex, NULL );
        m_sockfd = -1;
        m_bNet = false;
    }

    unsigned int currentMs() {
        struct timeval time;
        gettimeofday(&time, NULL);
        return (unsigned int)((time.tv_sec * 1000) + (time.tv_usec / 1000));
    }

    int make_socket_non_blocking (int sfd)
    {
        int flags, s;

        flags = fcntl (sfd, F_GETFL, 0);
        if (flags == -1)
        {
            perror ("fcntl");
            return -1;
        }

        flags |= O_NONBLOCK;
        s = fcntl (sfd, F_SETFL, flags);
        if (s == -1)
        {
            perror ("fcntl");
            return -1;
        }

        return 0;
    }

    static int create_and_bind (char *port)
    {
        struct addrinfo hints;
        struct addrinfo *result, *rp;
        int s, sfd;


        //signal
        struct sigaction act;
        act.sa_handler = SIG_IGN;
        sigaction(SIGPIPE, &act, NULL) ;

        memset (&hints, 0, sizeof (struct addrinfo));
        hints.ai_family = AF_UNSPEC;     // Return IPv4 and IPv6 choices 
        hints.ai_socktype = SOCK_STREAM; // We want a TCP socket 
        hints.ai_flags = AI_PASSIVE;     // All interfaces 

        s = getaddrinfo (NULL, port, &hints, &result);
        if (s != 0)
        {
            fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
            return -1;
        }

        for (rp = result; rp != NULL; rp = rp->ai_next)
        {
            sfd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (sfd == -1)
                continue;

            int opt = 1;
            if( setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT ,(const void*)&opt, sizeof(int)) != 0){
                fprintf (stderr, "Could not set SO_REUSEADDR errno: %d\n",errno);
                return -1;
            }

            s = bind (sfd, rp->ai_addr, rp->ai_addrlen);
            if (s == 0)
            {
                break;
            }

            close (sfd);
        }

        if (rp == NULL)
        {
            fprintf (stderr, "Could not bind: %s errno: %d\n",port, errno);
            return -1;
        }

        freeaddrinfo (result);

        return sfd;
    }

    void netServer::destroy() {
        connObjMgr::g_pConnMgr->destroy();
        usleep(100000);
        close (m_sockfd);
    }

    int netServer::initSock(char *port) {
        int sfd, s;

        sfd = create_and_bind (port);
        if (sfd == -1)
            return -1;

        s = make_socket_non_blocking (sfd);
        if (s == -1)
            return -2;
        m_sockfd = sfd;

        s = listen (m_sockfd, SOMAXCONN);
        if (s == -1)
        {
            perror ("listen");
            return -3;
        }

        //init epoll
        initEpoll();
        epAddFd(m_sockfd);
        return 0;
    }

    int netServer::initEpoll(){
        m_epollfd = epoll_create1 (0);
        if (m_epollfd == -1)
        {
            perror ("epoll_create failed");
            return 1;
        }
        return 0;
    }

    void netServer::setName(const char* pname){
        strcpy(m_name, pname);
        if( strstr(pname, "net") ){
            m_bNet = true;
        }else{
            m_bNet = false;
        }
    }
    int netServer::epAddFd(int fd, char* pname){
        if(pname != NULL){
            if(strlen(pname)!=0){
                m_rpcFdMap[fd] = string(pname);
            }
        }
        struct epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLET;
        int s = epoll_ctl (m_epollfd, EPOLL_CTL_ADD, fd, &event);
        if (s == -1)
        {
            perror ("epoll_ctl");
            return 1;
        }
        return 0;
    }

    void* netServer::netThreadFun( void *param) {
        netServer *pthis = (netServer*) param;

        /*
           queue<connObj*> que;
           for(int i=0;i<4000;i++){
           que.push( new connObj(i) );
           }
           LOG("connobj sizeof: %d que size: %d\n", sizeof(connObj), que.size());
           */

        // Buffer where events are returned 
        struct epoll_event *events;
        events = (epoll_event*) calloc (MAXEVENTS, sizeof( epoll_event) );

        LOG("netServer::netThreadFun started succ");
        // The event loop 
        while (g_run)
        {
            int n, i;

            do{
                n = epoll_wait (pthis->m_epollfd, events, MAXEVENTS, -1);
            }while(n<0&&errno == EINTR );
            for (i = 0; i < n; i++)
            {
                if ((events[i].events & EPOLLERR) ||
                        (events[i].events & EPOLLHUP) ||
                        (!(events[i].events & EPOLLIN)))
                {
                    // An error has occured on this fd, or the socket is not
                    //   ready for reading (why were we notified then?) 
                    pthis->appendConnClose(events[i].data.fd);
                    continue;
                }

                else if (pthis->m_sockfd == events[i].data.fd)
                {
                    while (1)
                    {
                        struct sockaddr in_addr;
                        socklen_t in_len;
                        int infd, s;
                        char hbuf[NI_MAXHOST]={0}, sbuf[NI_MAXSERV]={0};

                        in_len = sizeof in_addr;
                        infd = accept (pthis->m_sockfd, &in_addr, &in_len);
                        if (infd == -1)
                        {
                            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                            {
                                break;
                            }
                            else
                            {
                                perror ("accept");
                                break;
                            }
                        }

                        s = getnameinfo (&in_addr, in_len,
                                hbuf, sizeof hbuf,
                                sbuf, sizeof sbuf,
                                NI_NUMERICHOST | NI_NUMERICSERV);
                        if (s == 0)
                        {
                            //LOG("%.1f Accepted connection on descriptor %d " "(host=%s, port=%s)\r\n",getms()/1000.0, infd, hbuf, sbuf);
                        }

                        s = make_socket_non_blocking (infd);
                        if (s == -1)
                            abort ();

                        pthis->epAddFd(infd);
                        //new connection maked
                        pthis->appendConnNew(infd, (char*)hbuf, (char*)sbuf);
                    }
                    continue;
                }
                else
                {
                    pthis->appendDataIn(events[i].data.fd);

                }
            }
        }

        free (events);

        close (pthis->m_sockfd);
        LOG("exit epoll thread");
        LOG("netServer::netThreadFun quit succ");

        return NULL;
    }

    void netServer::queueProcessFun(){
        queue<NET_OP_ST *> queNew;

        pthread_mutex_lock(mutex);
        while(!this->m_netQueue.empty()){
            NET_OP_ST *pst = m_netQueue.front();
            m_netQueue.pop();
            if(pst==NULL){
                continue;
            }
            if(pst->op == NEW_CONN){
                queNew.push(pst);
            }else if(pst->op == DATA_IN){
                m_readFdMap[pst->fd] = true;
                delete pst;
            }else if(pst->op == QUIT_CONN){
                connObjMgr::g_pConnMgr->DelConn(pst->fd);
                delete pst;
            }
        }
        pthread_mutex_unlock(mutex);

        connObjMgr::g_pConnMgr->CreateConnBatch(&queNew, m_bNet);
        qpsMgr::g_pQpsMgr->updateQps(3, m_readFdMap.size());

        queue<int> delLst;
        //process all read event
        int fd, ret;
        for(map<int,bool>::iterator iter = m_readFdMap.begin(); iter != m_readFdMap.end(); iter++){
            fd = (int) iter->first;
            connObj *pconn = connObjMgr::g_pConnMgr->GetConn( fd );
            if (pconn == NULL){
                delLst.push(fd);
                continue;
            }
            ret = pconn->OnRead();
            if( ret == 0 ){
                delLst.push(fd);
            }
        }
        while(!delLst.empty()){
            int nfd = delLst.front();
            delLst.pop();
            m_readFdMap.erase(nfd);
        }

        //process all write event
        connObjMgr::g_pConnMgr->processAllWrite();
        tcpclientMgr::m_sInst->processAllRpcobj();

        qpsMgr::g_pQpsMgr->dumpQpsInfo();
        //connObjMgr::g_pConnMgr->ChkConnTimeout();
    }

    void netServer::appendSt(NET_OP_ST *pst, bool bmtx){
        if(bmtx){
            pthread_mutex_lock(mutex);
        }
        if( m_rpcFdMap.find(pst->fd) != m_rpcFdMap.end()){
            pst->bRpc = true;
            m_netQueueRpc.push(pst);
        }else{
            pst->bRpc = false;
            m_netQueue.push(pst);
        }

        if(bmtx){
            pthread_mutex_unlock(mutex);
        }
    }

    void netServer::appendDataIn(int fd){
        NET_OP_ST *pst = new NET_OP_ST();
        memset(pst,0, sizeof(NET_OP_ST));
        pst->op = DATA_IN;
        pst->fd = fd;
        this->appendSt(pst);
    }

    void netServer::appendConnNew(int fd, char *pip, char* pport){
        NET_OP_ST *pst = new NET_OP_ST();
        memset(pst,0, sizeof(NET_OP_ST));
        pst->bRpc = false;
        pst->op = NEW_CONN;
        pst->fd = fd;
        sprintf(pst->paddr, "%s:%s", pip, pport);        
        this->appendSt(pst);
    }
    void netServer::appendConnClose(int fd, bool bmtx){
        NET_OP_ST *pst = new NET_OP_ST();
        memset(pst,0, sizeof(NET_OP_ST));
        pst->op = QUIT_CONN;
        pst->fd = fd;
        this->appendSt(pst, bmtx);
    }


    char* netServer::GetOpType(NET_OP op){
        if( op == NEW_CONN){
            return (char*)"NEW_CONN";
        }else if( op == DATA_IN ) {
            return (char*)"DATA_IN";
        }else if( op == QUIT_CONN) {
            return (char*)"QUIT_CONN";
        }
        return (char*)"";
    }


    void netServer::queueProcessRpc(){
        queue<int> queNew;
        pthread_mutex_lock(mutex);

        while(!this->m_netQueueRpc.empty()){
            NET_OP_ST *pst = m_netQueueRpc.front();
            m_netQueueRpc.pop();
            if(pst==NULL){
                continue;
            }
            if(pst->op == NEW_CONN){
            }else if(pst->op == DATA_IN){
                m_readFdMap[pst->fd] = true;
            }else if(pst->op == QUIT_CONN){
                tcpclientMgr::m_sInst->DelConn(pst->fd);
            }
            delete pst;
        }
        //LOG("called queueProcessRpc m_readFdMap size: %d\n", m_readFdMap.size());

        pthread_mutex_unlock(mutex);

        queue<int> delLst;
        //process all read event
        int fd, ret;
        for(map<int,bool>::iterator iter = m_readFdMap.begin(); iter != m_readFdMap.end(); iter++){
            fd = (int) iter->first;
            tcpclient *pconn = tcpclientMgr::m_sInst->getTcpClientByFd(fd);
            if (pconn == NULL){
                delLst.push(fd);
                continue;
            }
            ret = pconn->OnRead();
            delLst.push(fd);
            pconn->dealReadBuffer();
            pconn->dealSend();
        }

        while(!delLst.empty()){
            int nfd = delLst.front();
            delLst.pop();
            m_readFdMap.erase(nfd);
        }

    }

}