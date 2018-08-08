#ifndef __HEADER__LOGGER__
#define __HEADER__LOGGER__
#include<stdarg.h>  
#include<iostream>  
#include<queue>  
using namespace std;  

#define LOG logger::info

#define LOG_SIZE 4096
namespace net{
    class logst {
        char mem[LOG_SIZE];
        public:
        logst();
        char* getptr();
        char* getwpos();
        int getcap();
    };

    class logger {
            char m_name[512];
        public:
            FILE* m_fp;
            queue<logst*> m_queueLst;
            pthread_mutex_t *mutex ; 
            logger(char*);
            static logger* m_inst;
            void AppendLog(logst* p);
            void DoOpen();
            static void info(const char *format,...) ;
            pthread_t init();
            static void* threadFun( void* param );
    };


}
#endif
