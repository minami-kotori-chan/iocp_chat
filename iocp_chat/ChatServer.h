#pragma once
#include "Iocp.h"

class ChatServer : public IocpServer
{
public:

protected:
	virtual void OnConnect(UINT32 idx);
	virtual void OnDisConnect(UINT32 idx);
	virtual void OnRecv(UINT32 idx);
	virtual void OnSendComplete(UINT32 idx);

	virtual void Start();//서버의 모든 초기화 완료이후 호출되는 함수
private:

};