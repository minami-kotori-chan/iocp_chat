#include "ChatServer.h"

void ChatServer::OnConnect(UINT32 idx)
{

}

void ChatServer::OnDisConnect(UINT32 idx)
{

}

void ChatServer::OnRecv(UINT32 idx)
{
	//char* pStr = "Hello";
	SendData(idx,(char*)"Hello",6);
}

void ChatServer::OnSendComplete(UINT32 idx)
{

}

void ChatServer::Start()
{

}