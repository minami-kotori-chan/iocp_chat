#pragma once

#include "IOCP/Iocp.h"
#include "ClientSession.h"

class ChatServer : public IocpServer
{
public:

protected:

	//////////////////////////
	//IOCP코어의 가상함수 override
	//////////////////////////
	virtual void OnConnect(UINT32 idx);
	virtual void OnDisConnect(UINT32 idx);
	virtual void OnRecv(UINT32 idx,char* pData, UINT32 pDataSize);
	virtual void OnSendComplete(UINT32 idx);

	virtual void Start(UINT32 MaxClientCnt);//서버의 모든 초기화 완료이후 호출되는 함수


private:

	ClientSessionManager ClientManager;
};