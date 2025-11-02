#pragma once

#include "IOCP/Iocp.h"
#include "ClientSession.h"
#include "DBManager.h"
#include "DelegateManager.h"
#include "ResultQueManager.h"

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

	virtual void Start(UINT32 MaxClientCnt);
	virtual void OnStopServer();
	//서버의 모든 초기화 완료이후 호출되는 함수


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

	void ProcessLogoutResult(LPacket& packet);

	bool DBResultThreadRun = true;
	bool PacketResultThreadRun = true;
	ClientSessionManager ClientManager;
	DBManager dbManager;

	std::vector<std::thread> DBResultThreads;
	std::vector<std::thread> PacketResultThreads;

	std::unordered_map<DB_TYPE, void (ChatServer::*)(DB_Result&)> DBResultMap;
	std::unordered_map<PACKET_ID, void (ChatServer::*)(LPacket&)> PacketResultMap;

	ResultQueManager RQueManager;
	
	DelegateManager<void, LPacket&> delegateManager;
};