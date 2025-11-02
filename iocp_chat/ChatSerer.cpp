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
	ClientManager.SetDelegate(&delegateManager);

	SetDBManager();
	CreateDBResultThread();

	ClientManager.BindResultQue(&RQueManager);
}

void ChatServer::SetDBManager()
{
	char user[20];
	char passwd[20];
	printf("DB Id를 입력하세요 : ");
	scanf_s("%s",user,sizeof(user));
	printf("DB pw를 입력하세요 : ");
	scanf_s("%s", passwd, sizeof(passwd));

	dbManager.Init("127.0.0.1",user,passwd,3306);
}

void ChatServer::CreateDBResultThread(UINT32 Threadcnt)
{
	for (UINT32 i = 0; i < Threadcnt; i++) {
		DBResultThreads.emplace_back([this]() {ProcessDBResult(); });
	}
	
}

void ChatServer::CreatePacketResultThread(UINT32 Threadcnt)
{
	for (UINT32 i = 0; i < Threadcnt; i++) {
		PacketResultThreads.emplace_back([this]() {ProcessPacketResult(); });
	}
}

void ChatServer::ProcessDBResult()
{
	while (DBResultThreadRun)
	{
		std::optional<DB_Result> DbResult;
		DbResult = dbManager.PopResultQue();
		if (!DbResult.has_value()) break;
		DB_Result dbResult = DbResult.value();
		if (DBResultMap.find(dbResult.Dtype) != DBResultMap.end())
		{
			(this->*(DBResultMap[dbResult.Dtype]))(dbResult);
		}
	}
}

void ChatServer::ProcessPacketResult()
{
	while (PacketResultThreadRun)
	{
		std::optional<LPacket> packetResult;
		packetResult = RQueManager.PopResultQue();
		if (!packetResult.has_value()) break;
		LPacket pResult = packetResult.value();
		if (PacketResultMap.find(pResult.PacketId) != PacketResultMap.end())
		{
			(this->*(PacketResultMap[pResult.PacketId]))(pResult);
		}
	}
}

void ChatServer::BindOnResultMap()
{
	DBResultMap[DB_TYPE::LOGIN_REQUEST] = &ChatServer::ProcessLoginResult;
	DBResultMap[DB_TYPE::SIGNUP_REQUEST] = &ChatServer::ProcessSignUpResult;
	DBResultMap[DB_TYPE::DELETE_USER_REQUEST] = &ChatServer::ProcessDeleteUserResult;

	PacketResultMap[PACKET_ID::LOGOUT_REQUEST] = &ChatServer::ProcessLogoutResult;
}




void ChatServer::ProcessLoginResult(DB_Result& DResult)
{
	ResponsePacket Rpacket;
	Rpacket.PacketId = PACKET_ID::LOGIN_RESPONSE;
	Rpacket.PacketSize = sizeof(ResponsePacket);
	Rpacket.Success = DResult.QueryResult;
	SendData(DResult.ClientSessionIdx, (char*) & Rpacket, sizeof(Rpacket));
	ClientManager.OnLoginSuccess(DResult.ClientSessionIdx, DResult.UserName,32);
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
void ChatServer::ProcessLogoutResult(LPacket& packet)
{
	ResponsePacket Rpacket;
	Rpacket.PacketId = PACKET_ID::LOGIN_RESPONSE;
	Rpacket.PacketSize = sizeof(ResponsePacket);
	Rpacket.Success = true;//로그아웃이 실패할 일은 존재하지않음
	SendData(packet.ClientIdx, (char*)&Rpacket, sizeof(Rpacket));
}


void ChatServer::CloseDBResultThread()
{
	DBResultThreadRun = false;
	dbManager.CloseResultQue();
	for (auto& th : DBResultThreads) {
		if (th.joinable()) {
			th.join();
		}
	}
}

void ChatServer::ClosePacketResultThread()
{
	PacketResultThreadRun = false;
	RQueManager.ResultQueManagerStop();
	for (auto& th : PacketResultThreads) {
		if (th.joinable()) {
			th.join();
		}
	}
}

void ChatServer::OnStopServer()
{
	ClientManager.StopManager();
	dbManager.CloseThread();
	CloseDBResultThread();
	ClosePacketResultThread();
}