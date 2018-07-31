debug:
	rm -rf nets
	g++ -g ./kcpsess/sessServer.cpp ./kcpsess/ikcp.c *.cpp ./core/*.cpp ./core/pb/*.cc -lpthread -o nets -lprotobuf -L./lib 

release:
	g++ *.cpp -lpthread -o nets 

kcp:
	g++ ./kcpsess/ep_server.cpp ./kcpsess/sessServer.cpp ./kcpsess/ikcp.c *.cpp ./core/*.cpp ./core/pb/*.cc -o kcp -lpthread -lprotobuf -L./lib

profile:
	g++ -g -fno-inline  main.cpp net.cpp conn.cpp connmgr.cpp recvBuff.cpp qps.cpp -lpthread -o nets 


clean:
	rm -rf nets
