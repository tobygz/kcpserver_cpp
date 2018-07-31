#include "cfg.h"
#include "unistd.h"
#include "log.h"

/*

   21009
   gate1,"127.0.0.1",20000
   game1,"127.0.0.1",21020
   */
namespace net{
    serverCfg* serverCfg::m_gInst = new serverCfg();

    void serverCfg::tostring(char* p){
        sprintf(p,"port: %s\n", listenPort );
        for(int i=0; i<nodeLst.size(); i++){
            nodeCfg *pnode = nodeLst[i];
            sprintf(p,"%sname: %s ip: %s port: %s\n", p, pnode->nodeName, pnode->ip, pnode->port );
        }
    }

    void serverCfg::init(const char* pstr){
        sprintf(m_servTag,"start_%s_cfg", pstr );
        char buffer[1024]={0};
        getcwd(buffer,1024);
        sprintf(buffer,"%s/conf/cfg.ini",buffer);
        printf("file:%s\n", buffer);
        FILE *fp = fopen(buffer,"r");
        if(!fp){
            printf("[FATAL] cfg.init open failed\n");
            return;
        }
        char buf[4096];
        int num = fread(buf,1, 4096, fp);
        if(num<=0){
            printf("[FATAL] cfg.init read failed %d\n", num);
            return;
        }
        //parse file
        int idx = 0;
        bool bskip = false;
        bool bstart = false;
        vector<char*> allStr;

        char* ptk = strtok(buf,"\n");
        while(ptk){
            if( ptk[0] == '#' ){
                ptk = strtok(NULL,"\n");
                continue;
            }
            if( strstr(ptk, "start") != NULL ){
                if( strstr(ptk, m_servTag) != NULL ){
                    bstart = true;
                }else{
                    bstart = false;
                }
            }
            if(!bstart){
                ptk = strtok(NULL,"\n");
                continue;
            }
            if( ptk[0] == '['){
                if( strstr(ptk,"true") ==NULL){
                    bskip = true;
                }else{
                    bskip = false;
                }
            }
            if(bskip){
            ptk = strtok(NULL,"\n");
                continue;
            }else{
                if(ptk&& ptk[0] != '[' && ptk[0] !='#'){
                    allStr.push_back(ptk);
                }
            }
            ptk = strtok(NULL,"\n");
        }

        for(int i=0; i<allStr.size(); i++){
            char *pt = allStr[i];
            if( pt[0] != 'g' && pt[0] != 'k' ){
                strcpy(listenPort, pt );
            }else{
                nodeCfg *pnode = new nodeCfg;
                char *ptk1 = strtok(pt,",");
                idx = 0;
                while(ptk1){
                    if(idx == 0 ){
                        strcpy(pnode->nodeName, ptk1 );
                    }else if( idx == 1 ){
                        strcpy(pnode->ip, ptk1 );
                    }else if( idx == 2 ){
                        strcpy(pnode->port, ptk1 );
                        nodeLst.push_back( pnode );
                    }
                    idx++;
                    ptk1 = strtok(NULL,",");
                }
            }
        }

        printf("loadcfg listen: %s\n", listenPort );
        for(int i=0; i<nodeLst.size(); i++){
            nodeCfg *pnode = nodeLst[i];
            printf("loadcfg name: %s ip: %s port: %s\n", pnode->nodeName, pnode->ip, pnode->port );
        }

    }

}
/*
   int main(){
   serverCfg s(3);
   char info[512] = {0};
   s.tostring(info);
   printf(info);
   return 0;
   }
   */
