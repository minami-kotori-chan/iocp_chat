#pragma once

#include "IOCP/Iocp.h"
#include "ClientSession.h"
#include "DBManager.h"
#include "DelegateManager.h"
#include "ResultQueManager.h"
#include "PacketSenderInterface.h"

class ChatServer : public IocpServer, PacketSenderInterface
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

	virtual void Start(UINT32 MaxClientCnt);
	virtual void OnStopServer();
	//서버의 모든 초기화 완료이후 호출되는 함수

	//Send용 인터페이스 오버라이딩
	virtual void SendData(UINT32 idx, char* pData, int Psize) { IocpServer::SendData(idx, pData, Psize); }//추후 필요한 경우 room에서 resultque거치지 않고 직접 send가능하게 하기 위해 작성

private:
	void SetDBManager();
	void CreateDBResultThread(UINT32 Threadcnt=1);
	void CreatePacketResultThread(UINT32 Threadcnt = 1);

	void ProcessDBResult();
	void ProcessPacketResult();

	void BindOnResultMap();
	void CloseDBResultThread();
	void ClosePacketResultThread();

	void ProcessLoginResult(DB_Result& DResult);
	void ProcessSignUpResult(DB_Result& DResult);
	void ProcessDeleteUserResult(DB_Result& DResult);

	void ProcessLogoutResult(LPacketResult& packet);

	void SendResponsePacket(LPacketResult& packet);

	bool DBResultThreadRun = true;
	bool PacketResultThreadRun = true;
	ClientSessionManager ClientManager;
	DBManager dbManager;

	std::vector<std::thread> DBResultThreads;
	std::vector<std::thread> PacketResultThreads;

	std::unordered_map<DB_TYPE, void (ChatServer::*)(DB_Result&)> DBResultMap;
	std::unordered_map<PACKET_ID, void (ChatServer::*)(LPacketResult&)> PacketResultMap;

	ResultQueManager RQueManager;
	
	DelegateManager<void, LPacket&> delegateManager;
};