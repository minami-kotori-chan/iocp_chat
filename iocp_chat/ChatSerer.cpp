#include "ChatServer.h"

void ChatServer::OnConnect(UINT32 idx)
{
	ClientManager.PushSystemPacket(idx,PACKET_ID::CONNECT_REQUEST);
}

void ChatServer::OnDisConnect(UINT32 idx)
{

}

void ChatServer::OnRecv(UINT32 idx, char* pData, UINT32 pDataSize)
{
	SendData(idx,(char*)"Hello",6);
	ClientManager.PushRecvPacket(idx,pData,pDataSize);
}

void ChatServer::OnSendComplete(UINT32 idx)
{

}

void ChatServer::Start(UINT32 MaxClientCnt)
{
	ClientManager.Init(MaxClientCnt);
}