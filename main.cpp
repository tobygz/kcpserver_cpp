//https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/epoll-example.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <sys/time.h>


#include "net.h"
#include "tcpclient.h"
#include "cfg.h"
#include "log.h"
#include "conn.h"
#include "qps.h"
#include "./core/room.h"
#include "./kcpsess/sessServer.h"

using namespace net;

pthread_t InitRpcThread(bool bread);
pthread_t InitNetListen(char* port);

//for game
pthread_t InitGameListen(char* port);
pthread_t InitKcpListen();
static IINT64 diffTime(timeval te, timeval ts);

void onQuit(int sigval){
    LOG("called onQuit sigval: %d", sigval);
    if( netServer::g_netServer->isNet() == false && KCPServer::m_sInst){
        //LOG("allsessid: %s",KCPServer::m_sInst->getAllSessid().c_str());
        usleep(100000);
    }
    if(sigval == SIGINT || sigval == SIGQUIT ) {
        netServer::g_run = false;
    }
}

int main(int argc, char*argv[]){

    netServer::g_run = true;

    signal(SIGINT, onQuit);
    signal(SIGQUIT, onQuit);
    signal(SIGPIPE, SIG_IGN);

    if( argc < 2 ){
        printf("need args [net|game] \n");
        return 0;
    }

    if( strcmp(argv[1],"net") == 0 ){
        char *name = (char*)"net1";
        logger::m_inst = new logger(name);
        pthread_t log_pid = logger::m_inst->init();
        serverCfg::m_gInst->init("net");
        netServer::g_netServer->setName(name);
        //for listen socket
        pthread_t id = InitNetListen( serverCfg::m_gInst->getListen() );

        pthread_t rpcid_r = InitRpcThread(true);
        pthread_t rpcid_w = InitRpcThread(false);

        unsigned int ms = net::currentMs();
        while(1){
            if(!netServer::g_run){
                break;
            }
            ms = net::currentMs();
            netServer::g_netServer->queueProcessFun(ms);
            usleep(1000);
        }

        netServer::g_netServer->destroy();
        tcpclientMgr::m_sInst->destroy();

        pthread_cancel(rpcid_r);
        pthread_cancel(rpcid_w);
        pthread_cancel(log_pid);
        pthread_join(id, NULL);
        pthread_join(log_pid, NULL);
        pthread_join(rpcid_r, NULL);
        pthread_join(rpcid_w, NULL);
    }else{
        /*
         * loadcfg listen: 21021 //listen for rpc, for net
         * loadcfg name: gate1 ip: 127.0.0.1 port: 20000 //connect to gate
         * loadcfg name: kcp ip: 0.0.0.0 port: 10022 //listen kcp
         * */
        char *name = (char*)"game1";
        logger::m_inst = new logger(name);
        pthread_t log_pid = logger::m_inst->init();
        //for game
        serverCfg::m_gInst->init("game");
        netServer::g_netServer->setName(name);

        roomMgr::m_inst = new roomMgr;
        playerMgr::m_inst = new playerMgr;
        KCPServer::m_sInst = new KCPServer;

        //for listen socket
        pthread_t id = InitGameListen( serverCfg::m_gInst->getListen() );
        pthread_t id_kcp = InitKcpListen();
        pthread_t rpcid_r = InitRpcThread(true);
        pthread_t rpcid_w = InitRpcThread(false);

        unsigned int ms = net::currentMs();
        unsigned int lastMs = ms;
        struct timeval tm_s, tm_e;
        while(1){
            if(!netServer::g_run){
                break;
            }

            ms = net::currentMs();
            if(ms-lastMs>3){
                netServer::g_netServer->queueProcessFun(ms);
                if(connRpcObj::m_inst){
                    //net->game
                    connRpcObj::m_inst->Update(ms);
                }
                lastMs = ms;

                //process kcp datas
                KCPServer::m_sInst->Update(ms);        
            }
            roomMgr::m_inst->Update(ms);

            qpsMgr::g_pQpsMgr->updateQps(1, 1);

            usleep(20);
        }

        if(KCPServer::m_sInst){
            KCPServer::m_sInst->Destroy();
        }
        netServer::g_netServer->destroy();
        //disconn from gate1
        if(connRpcObj::m_inst){
            connRpcObj::m_inst->destroy();
        }

        pthread_cancel(id);
        pthread_cancel(id_kcp);
        pthread_cancel(log_pid);
        pthread_cancel(rpcid_r);
        pthread_cancel(rpcid_w);

        pthread_join(id, NULL);
        pthread_join(id_kcp, NULL);
        pthread_join(log_pid, NULL);
        pthread_join(rpcid_r, NULL);
        pthread_join(rpcid_w, NULL);
    }
    return 0;
}

pthread_t InitRpcThread(bool bread){
    pthread_t id;
    int ret;
    if(bread){
        ret=pthread_create(&id,NULL, &tcpclientMgr::readThread, NULL);
    }else{
        ret=pthread_create(&id,NULL, &tcpclientMgr::writeThread, NULL);
    }
    if(ret!=0){
        LOG("Create pthread error!");
        exit (1);
    }
    return id;
}

pthread_t InitNetListen(char* port){
    if(netServer::g_netServer->initSock(port) != 0){
        exit(0);
    }

    for(int i=0; i<serverCfg::m_gInst->nodeLst.size(); i++){
        nodeCfg* p = serverCfg::m_gInst->nodeLst[i];
        int port = atoi(p->port);
        tcpclientMgr::m_sInst->createTcpclient( p->nodeName, p->ip, port );
    }

    pthread_t id;
    int i,ret;
    ret=pthread_create(&id,NULL, &netServer::netThreadFun , netServer::g_netServer);
    if(ret!=0){
        LOG("Create pthread error!");
        exit (1);
    }
    return id;
}


pthread_t InitGameListen(char* port){
    if(netServer::g_netServer->initSock(port) != 0){
        exit(0);
    }

    //connect to gate1
    for(int i=0; i<serverCfg::m_gInst->nodeLst.size(); i++){
        nodeCfg* p = serverCfg::m_gInst->nodeLst[i];
        if( strcmp(p->nodeName,"kcp") == 0 ){
            continue;
        }
        int port = atoi(p->port);
        tcpclientMgr::m_sInst->createTcpclient( p->nodeName, p->ip, port );
    }

    pthread_t id;
    int i,ret;
    ret=pthread_create(&id,NULL, &netServer::netThreadFun , netServer::g_netServer);
    if(ret!=0){
        LOG("Create pthread error!");
        exit (1);
    }
    return id;
}

pthread_t InitKcpListen(){
    for(int i=0; i<serverCfg::m_gInst->nodeLst.size(); i++){
        nodeCfg* p = serverCfg::m_gInst->nodeLst[i];
        if( strcmp(p->nodeName,"kcp") == 0 ){
            int port = atoi(p->port);
            return KCPServer::m_sInst->Listen(port);        
        }
    }
}


