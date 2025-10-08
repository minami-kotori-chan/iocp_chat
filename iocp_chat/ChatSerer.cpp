#include "ChatServer.h"

void ChatServer::OnConnect(UINT32 idx)
{

}

void ChatServer::OnDisConnect(UINT32 idx)
{

}

void ChatServer::OnRecv(UINT32 idx, char* pData, UINT32 pDataSize)
{
	SendData(idx,(char*)"Hello",6);
}

void ChatServer::OnSendComplete(UINT32 idx)
{

}

void ChatServer::Start(UINT32 MaxClientCnt)
{

}

void ChatServer::CreateUserSession(UINT32 MaxClientCnt)
{

}
