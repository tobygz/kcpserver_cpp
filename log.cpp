#include "log.h"
#include<stdio.h>  
#include<string.h>  
#include<unistd.h>  
#include<time.h>  

#include "net.h"

using namespace std;  

namespace net {

    logger* logger::m_inst = NULL;
    logst::logst(){
        memset(mem,0,LOG_SIZE);
        time_t t = time(NULL);
        struct tm *ptminfo;
        strftime(mem, sizeof(mem), "%Y-%m-%d %H:%M:%S",localtime(&t) );
        sprintf(mem, "%s[%d]: ",mem, t);
    }
    char* logst::getptr(){ return mem + strlen(mem); }
    char* logst::getwpos(){ return mem; }
    int logst::getcap(){ return LOG_SIZE - strlen(mem); }

    logger::logger(char *name){
        mutex = new pthread_mutex_t;
        pthread_mutex_init( mutex, NULL );
        //open file
        char path[2048] = {0};
        char tmppath[2048] = {0};
        getcwd(tmppath,2048);
        sprintf(path, "%s/log/%s.log",tmppath, name );
        m_fp = fopen(path, "w+");
        if(!m_fp){
            printf("open logger file failed\n");
            return;
        }
    }
    void logger::AppendLog(logst* p){
        pthread_mutex_lock(mutex);
        m_queueLst.push(p);
        pthread_mutex_unlock(mutex);
    }

    void logger::info(const char *format,...)
    {
        logst *pst = new logst;
        va_list args;
        va_start(args,format);
        vsnprintf(pst->getptr(), pst->getcap(),format,args);
        va_end(args);
        logger::m_inst->AppendLog(pst);
    }
    pthread_t logger::init(){
        pthread_t id;
        pthread_create(&id, NULL, &logger::threadFun, NULL);
        return id;
    }

    void* logger::threadFun( void* param ){
        printf("log threadFun started \n");
        queue<logst*> logQue;
        const char* ln = "\n";
        while(netServer::g_run){

            pthread_mutex_lock(m_inst->mutex);
            while(!m_inst->m_queueLst.empty()){
                logst *st = m_inst->m_queueLst.front();
                m_inst->m_queueLst.pop();
                logQue.push(st);
            }
            pthread_mutex_unlock(m_inst->mutex);

            while(!logQue.empty()){
                logst *st = logQue.front();
                logQue.pop();
                fwrite( st->getwpos(), 1, strlen( st->getwpos()) , m_inst->m_fp );
                printf("%s\n", st->getwpos() );
                fwrite(ln, 1, strlen( ln ), m_inst->m_fp);
                delete st;
            }

            usleep(1000);
            fflush(m_inst->m_fp);
        }
        fclose(m_inst->m_fp);
        printf("log threadFun quit\n");
    }

}
