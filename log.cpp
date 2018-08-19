#include "log.h"
#include<stdio.h>  
#include<string.h>  
#include<unistd.h>  
#include<time.h>  

#include "net.h"

using namespace std;  

#define MAX_FILE_LEN 268435456 //1024*1024*256

#define LOGST_SIZE 1024*2 //1024*1024*1024
namespace net {

    logger* logger::m_inst = NULL;
    logst::logst(){ }

    void logst::init(){
        memset(mem,0,LOG_SIZE);
        timeval curTime;
        gettimeofday(&curTime, NULL);
        int ms = curTime.tv_usec / 1000;
        strftime(mem, sizeof(mem), "%Y%m%d %H:%M:%S", localtime(&curTime.tv_sec));
        sprintf(mem, "%s:%03d ",mem, ms);
    }
    char* logst::getptr(){ return mem + strlen(mem); }
    char* logst::getwpos(){ return mem; }
    int logst::getcap(){ return LOG_SIZE - strlen(mem); }

    logger::logger(char *name){
        mutex = new pthread_mutex_t;
        pthread_mutex_init( mutex, NULL );

        mutexPool = new pthread_mutex_t;
        pthread_mutex_init( mutexPool, NULL );

        initPool();
        strcpy(m_name, name );
        m_fp = NULL;
        //open file
        DoOpen();
    }

    void logger::initPool(){
        pthread_mutex_lock(mutex);
        for(int i=0; i<LOGST_SIZE; i++){
            logst *p = new logst;
            memset(p,0,sizeof(logst));
            m_pool.push(p);
        }
        pthread_mutex_unlock(mutex);

    }
    void logger::pushSt(logst* p){
        pthread_mutex_lock(mutex);
        m_pool.push(p);
        memset(p,0,sizeof(logst));
        pthread_mutex_unlock(mutex);

    }
    logst* logger::popSt(){
        logst *p = NULL;
        pthread_mutex_lock(mutex);
        if(m_pool.size() == 0 ){
            p = new logst;
            p->init();
        }else{
            p = m_pool.front();
            p->init();
            m_pool.pop();
        }
        pthread_mutex_unlock(mutex);
        return p;

    }

    void logger::DoOpen(){
        if(m_fp){
            fflush(m_fp);
            fclose(m_fp);
            m_fp = NULL;
        }

        time_t t = time(NULL);
        char tmstr[64] = {0};
        strftime(tmstr, sizeof(tmstr) - 1, "%Y%m%d_%H%M%S", localtime(&t));

        char path[2048] = {0};
        char tmppath[2048] = {0};
        getcwd(tmppath,2048);
        sprintf(path, "%s/log/%s_%s.log",tmppath, m_name, tmstr );
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
        logst *pst = logger::m_inst->popSt();
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
        unsigned long long ct = 0;
        while(netServer::g_run){

            pthread_mutex_lock(m_inst->mutex);
            while(!m_inst->m_queueLst.empty()){
                logst *st = m_inst->m_queueLst.front();
                m_inst->m_queueLst.pop();
                logQue.push(st);
            }
            pthread_mutex_unlock(m_inst->mutex);

            while(!logQue.empty()){
                ct++;
                logst *st = logQue.front();
                logQue.pop();
                fwrite( st->getwpos(), 1, strlen( st->getwpos()) , m_inst->m_fp );
                //printf("%s\n", st->getwpos() );
                fwrite(ln, 1, strlen( ln ), m_inst->m_fp);
                logger::m_inst->pushSt(st);
            }
            if( ct > MAX_FILE_LEN ){
                m_inst->DoOpen();
                ct = 0;
            }

            usleep(1000);
            fflush(m_inst->m_fp);
        }
        fclose(m_inst->m_fp);
        printf("log threadFun quit\n");
    }

}
