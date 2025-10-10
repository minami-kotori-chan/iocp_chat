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

void ChatServer::SetDBManager()
{
	char* user = nullptr;
	char* passwd = nullptr;
	printf("DB Id를 입력하세요 : ");
	scanf_s("%s",user);
	printf("DB pw를 입력하세요 : ");
	scanf_s("%s", passwd);

	dbManager.Init("127.0.0.1",user,passwd,3306);
	ResultQueCV=dbManager.GetResultQueLockCV();
	ResultQueLock = dbManager.GetResultQueLock();
}

void ChatServer::CreateDBResultThread(UINT32 Threadcnt)
{
	for (UINT32 i = 0; i < Threadcnt; i++) {
		DBResultThreads.emplace_back([this]() {ProcessDBResult(); });
	}
	
}

void ChatServer::ProcessDBResult()
{
	while (DBResultThreadRun)
	{
		DB_Result DbResult;
		{
			std::unique_lock<std::mutex> lock(*ResultQueLock);
			ResultQueCV->wait(lock, [this] {return !dbManager.ResultQueEmpty() || !DBResultThreadRun; });
			if (DBResultThreadRun == false) break;
			if (dbManager.ResultQueEmpty()) {
				continue;
			}
			DbResult = dbManager.PopResultQue();
		}
		if (DBResultMap.find(DbResult.Dtype) != DBResultMap.end())
		{
			(this->*(DBResultMap[DbResult.Dtype]))(DbResult);
		}
	}
}

void ChatServer::BindOnDBResultMap()
{
	DBResultMap[DB_TYPE::LOGIN_REQ] = &ChatServer::ProcessLoginResult;
	DBResultMap[DB_TYPE::SIGNUP_REQ] = &ChatServer::ProcessSignUpResult;
	DBResultMap[DB_TYPE::DELETE_USER_REQ] = &ChatServer::ProcessDeleteUserResult;
}

void ChatServer::ProcessLoginResult(DB_Result& DResult)
{
	ResponsePacket Rpacket;
	Rpacket.PacketId = PACKET_ID::LOGIN_RESPONSE;
	Rpacket.PacketSize = sizeof(ResponsePacket);
	Rpacket.Success = DResult.QueryResult;
	SendData(DResult.ClientSessionIdx, (char*) & Rpacket, sizeof(Rpacket));

}

void ChatServer::ProcessSignUpResult(DB_Result& DResult)
{
	ResponsePacket Rpacket;
	Rpacket.PacketId = PACKET_ID::SIGNUP_RESPONSE;
	Rpacket.PacketSize = sizeof(ResponsePacket);
	Rpacket.Success = DResult.QueryResult;
	SendData(DResult.ClientSessionIdx, (char*)&Rpacket, sizeof(Rpacket));
}

void ChatServer::ProcessDeleteUserResult(DB_Result& DResult)
{
	ResponsePacket Rpacket;
	Rpacket.PacketId = PACKET_ID::DELETE_USER_RESPONSE;
	Rpacket.PacketSize = sizeof(ResponsePacket);
	Rpacket.Success = DResult.QueryResult;
	SendData(DResult.ClientSessionIdx, (char*)&Rpacket, sizeof(Rpacket));
}


void ChatServer::OnStopServer()
{
	ClientManager.StopManager();
}