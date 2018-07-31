#ifndef __HEADER_rpcNetHandleR_BUFFER__
#define __HEADER_rpcNetHandleR_BUFFER__

#include <map>
#include "recvBuff.h"
#include <iostream>

using namespace std;
namespace net{


    //rpc net <=> gate\game
	class rpcNetHandle {

    protected:
        typedef int (rpcNetHandle::*PTRFUN)(rpcObj*);
        map<string,PTRFUN> m_mapHandler;
		public:

        rpcNetHandle();
        static rpcNetHandle *m_pInst;
        
        void init();
        void process(rpcObj* pobj);
        
        int onPing(rpcObj*);
        int onPushMsg2Client(rpcObj*);
        int onPushMsg2ClientAll(rpcObj*);
        int onForceCloseCliConn(rpcObj*);
        int onRegGamePid(rpcObj*);
        
	};

    class rpcGameHandle {

    protected:
        typedef int (rpcGameHandle::*PTRFUN)(rpcObj*);
        map<string,PTRFUN> m_mapHandler;

		public:
        static rpcGameHandle *m_pInst;
        
        rpcGameHandle();
        void init();
        void process(rpcObj* pobj);
        void process(msgObj* obj, int pid, int sessid);
        
        //from gate
        int NewRoom(rpcObj*);
        int OverRoom(rpcObj*);
        int OfflineP(rpcObj*);
        int EnterP(rpcObj*);
        int SvrFrameCmd(rpcObj*);
        int AllPReady(rpcObj*);

        //from net
        int Msg2game(rpcObj*);
        int TakeProxy(rpcObj*){}

        //from api
        int Api_2000(rpcObj*);
        int Api_2002(rpcObj*);
        int Api_2003(rpcObj*);
        int Api_2004(rpcObj*);


    };
}

#endif
