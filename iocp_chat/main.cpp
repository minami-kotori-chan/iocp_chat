#include "ChatServer.h"

const UINT16 SERVER_PORT = 11021;
const UINT16 MAX_CLIENT = 10000*2;

int main(void) {
	
	ChatServer IoServer;
	IoServer.InitSocket();
	IoServer.BindandListen(SERVER_PORT);
	IoServer.StartServer(MAX_CLIENT);
	printf("\n아무 키나 누를 때까지 대기합니다\n");
	getchar();

	IoServer.StopServer();
	
	return 0;
}