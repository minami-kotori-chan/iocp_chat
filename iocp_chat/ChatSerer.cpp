#include "ChatServer.h"

void ChatServer::OnConnect(UINT32 idx)
{
	ClientManager.PushSystemPacket(idx,PACKET_ID::CONNECT_REQUEST);
	MessagePacket mp;
	CopyMemory(mp.Msg,"hello",6);
	mp.PacketId = PACKET_ID::MESSAGE_RESPONSE;
	mp.PacketSize = sizeof(PacketHead) + 6;
	SendData(idx, (char*) &mp, mp.PacketSize);
}

void ChatServer::OnDisConnect(UINT32 idx)
{

}

void ChatServer::OnRecv(UINT32 idx, char* pData, UINT32 pDataSize)
{
	//SendData(idx,(char*)"Hello",6);
	ClientManager.PushRecvPacket(idx,pData,pDataSize);
}

void ChatServer::OnSendComplete(UINT32 idx)
{

}

void ChatServer::Start(UINT32 MaxClientCnt)
{
	ClientManager.Init(MaxClientCnt);
	ClientManager.SetDelegate(&delegateManager);
	ClientManager.SetSender(this);
	ClientManager.SetThroughput(LostPacketCount);

	SetDBManager();
	CreateDBResultThread();
	CreatePacketResultThread();
	ClientManager.BindResultQue(&RQueManager);
	CreateTPSThread();
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
	dbManager.BindingFuncOnDelegate(delegateManager);
	BindOnResultMap();
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

void ChatServer::CreateTPSThread()
{
	TPSThread = std::thread([this]() {CalcTPSThread(); });
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
		std::optional<LPacketResult> packetResult;
		packetResult = RQueManager.PopResultQue();
		if (!packetResult.has_value()) break;
		LPacketResult pResult = packetResult.value();
		if (PacketResultMap.find(pResult.PacketId) != PacketResultMap.end())
		{
			(this->*(PacketResultMap[pResult.PacketId]))(pResult);
		}
		else
		{
			SendResponsePacket(pResult);
			//printf("전송완료 packet id : %d \n", pResult.PacketId);
			Throughput++;
		}
	}
}

void ChatServer::CalcTPSThread()
{
	while (CalcTPSThreadRun)
	{
		printf("처리량 : %lld 수신 손실량 : %lld 전송 손실량 : %d\n", Throughput.load(), LostPacketCount.load(), GetLostSendPacekt());
		Throughput.store(0);
		LostPacketCount.store(0);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));//1초마다 측정
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
void ChatServer::ProcessLogoutResult(LPacketResult& packet)
{
	SendResponsePacket(packet);
}

void ChatServer::SendResponsePacket(LPacketResult& packet)
{
	ResponsePacket Rpacket;
	Rpacket.PacketId = packet.PacketId;
	Rpacket.PacketSize = sizeof(ResponsePacket);
	Rpacket.Success = packet.Success;
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

void ChatServer::CloseTPSThread()
{
	CalcTPSThreadRun = false;
	if (TPSThread.joinable()){
		TPSThread.join();
	}
}

void ChatServer::OnStopServer()
{
	ClientManager.StopManager();
	dbManager.CloseThread();
	CloseDBResultThread();
	ClosePacketResultThread();
	CloseTPSThread();
}