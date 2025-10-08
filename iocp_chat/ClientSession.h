#pragma once

#ifndef MAX_PACKET_SIZE//헤더가 포함되지 않았으면 포함하기 위해서 packet헤더의 매크로를 이용
#include "Packet.h"//클라이언트세션에서 버퍼의 패킷을 꺼낼때 한개씩 꺼내기 위해서는 패킷구조를 사용해야하는데 이를 위해 헤더 포함(설계변경을 한번 해서 iocp코어에 packet헤더가 있는 형태에서 애플리케이션단으로 넘어온거라 ifndef로 포함하는중 추후 ifndef는 삭제예정)
#endif // !MAX_PACKET_SIZE
#include <deque>
#include <unordered_map>
#include <functional>
#include <string>

enum class ClinetState : UINT16
{
	NONE=0,
	CONNECTED=1000,//연결은 됐는데 로그인은 안된 상태
	LOGIN=2000,
};

#define MAX_SEESION_BUFSIZE MAX_SOCKBUF * 8//IOCP코어의 소켓크기 * 8로 링버퍼 초과 막기

struct ClientSession
{
	UINT32 ClientIdx;//클라이언트 ID
	ClinetState CState;

	char SessionRecvBuf[MAX_SEESION_BUFSIZE];

	UINT16 BufHead;//buf Read
	UINT16 BufTail;//buf Write
	UINT16 BufDataSize;

	std::string UserName;//추후에 이것도 char배열로 만들어야 함 동적할당 최소화를 위해서


	ClientSession()
	{
		CState = ClinetState::NONE;
		BufHead = 0;
		BufTail = 0;
		BufDataSize = 0;
	}

	void SetDataOnBuf(char *pData, UINT16 pDataSize)//락필요
	{
		if (BufTail + pDataSize >= MAX_SEESION_BUFSIZE)
		{
			if (BufDataSize != 0) {
				CopyMemory(&SessionRecvBuf[0], &SessionRecvBuf[BufHead], BufDataSize);//읽어야하는 부분부터 복제
				BufTail = BufDataSize;
				BufHead = 0;
			}
			else {
				BufTail = 0;
				BufHead = 0;
			}
		}
		CopyMemory(&SessionRecvBuf[BufTail], pData, pDataSize);
		BufTail += pDataSize;
		BufDataSize += pDataSize;
	}

	void SetSystemDataOnBuf(PACKET_ID pId)
	{
		PacketHead pHead;
		pHead.PacketId = pId;
		pHead.PacketSize = sizeof(PacketHead);
		SetDataOnBuf((char*)&pHead, pHead.PacketSize);
	}

	LPacket GetDataOnBuf()//락필요
	{
		PacketHead* pHead = (PacketHead*)(&SessionRecvBuf[BufHead]);
		if (BufDataSize == 0 || pHead->PacketSize == 0) {
			return LPacket();
		}
		LPacket pInfo;
		pInfo.PacketId = pHead->PacketId;
		pInfo.PacketSize = pHead->PacketSize;
		pInfo.pData = &SessionRecvBuf[BufHead];
		pInfo.ClientIdx = ClientIdx;

		BufHead += pHead->PacketSize;
		BufDataSize -= pHead->PacketSize;
		return pInfo;
	}
	void OnConnect()
	{
		CState = ClinetState::CONNECTED;
		printf("\nClient Connected id : %d\n", ClientIdx);
	}
	void OnLogin(char *LoginName,UINT8 NameSize)
	{
		CState = ClinetState::LOGIN;
		UserName = LoginName;
	}

};

class ClientSessionManager
{
public:
	void Init(UINT32 MaxClientCnt)
	{
		for (UINT32 i = 0; i < MaxClientCnt; i++)
		{
			ClientSessions.emplace_back(new ClientSession());
			ClientSessions[i]->ClientIdx = i;
		}
		
		BindFunc();
		CreateProcessThreads();
	}
	ClientSession* GetClient(UINT32 idx)
	{
		return ClientSessions[idx];
	}

	void PushRecvPacket(UINT32 idx, char* pData, UINT16 pDataSize)
	{
		std::lock_guard<std::mutex> lock(RecvPacketQueLock);
		ClientSessions[idx]->SetDataOnBuf(pData, pDataSize);
		RecvPacketQueue.push_back(idx);
		RecvPacketCV.notify_one();
	}
	void PushSystemPacket(UINT32 idx, PACKET_ID pId)
	{
		std::lock_guard<std::mutex> lock(RecvPacketQueLock);
		ClientSessions[idx]->SetSystemDataOnBuf(pId);
		RecvPacketQueue.push_back(idx);
		RecvPacketCV.notify_one();
	}

	void StopManager()
	{
		RecvPacketThreadRun = false;
		RecvPacketCV.notify_all();
		for (auto& thread : ProcessRecvPacketThreads) {
			if (thread.joinable())
			{
				thread.join();
			}
		}
		RecvPacketQueue.clear();
	}

private:
	void CreateProcessThreads(UINT32 ThreadCnt=1)
	{
		for (UINT32 i = 0; i < ThreadCnt; i++) {
			ProcessRecvPacketThreads.emplace_back([this]() {ProcessRecvPacket(); });
		}
	}

	void BindFunc()//함수포인터 바인딩
	{
		RecvPacketFuncMap[(int)PACKET_ID::CONNECT_REQUEST] = &ClientSessionManager::OnConnect;//멤버함수포인터는 &가 필수임
		RecvPacketFuncMap[(int)PACKET_ID::LOGIN_REQUEST] = &ClientSessionManager::OnLogin;//멤버함수포인터는 &가 필수임
	}

	void ProcessRecvPacket()//패킷처리 스레드에서 호출하는 함수
	{
		while (RecvPacketThreadRun)
		{
			LPacket packet;
			bool IsValid = false;
			{
				std::unique_lock<std::mutex> lock(RecvPacketQueLock);

				RecvPacketCV.wait(lock, [this] { return !RecvPacketQueue.empty() || !RecvPacketThreadRun; });
				
				if (RecvPacketThreadRun == false) break;

				if (!RecvPacketQueue.empty()){
					packet = PopRecvPacket();
					IsValid = true;
				}

			}
			if(IsValid) PacketProcess(packet);
		}
	}

	void PacketProcess(LPacket& packet)//큐에서 데이터 꺼내고 실제 처리하는 함수
	{

		if (RecvPacketFuncMap.find((int)(packet.PacketId)) != RecvPacketFuncMap.end())
		{
			(this->*(RecvPacketFuncMap[(int)(packet.PacketId)]))(packet);//함수포인터 코드 iter로 바꾸는게 나을수도 있을듯
		}
		else
		{
			//식별할 수 없는 패킷 id
			printf("수신한 식별 불가능한 패킷 ID : %d", packet.PacketId);
		}
	}


	void OnConnect(LPacket& packet)
	{
		ClientSessions[packet.ClientIdx]->OnConnect();
	}

	void OnLogin(LPacket& packet)
	{
		LoginPacket* LoginP = (LoginPacket*)(packet.pData);

		ClientSessions[packet.ClientIdx]->OnLogin(LoginP->UserName, MAX_USERNAME_LENGTH);
	}

	LPacket PopRecvPacket()
	{
		UINT32 idx = RecvPacketQueue.front();
		RecvPacketQueue.pop_front();
		return ClientSessions[idx]->GetDataOnBuf();
	}

	bool RecvPacketThreadRun = true;
	std::vector<ClientSession*> ClientSessions;

	std::deque<UINT32> RecvPacketQueue;//동적할당이라서 불리하긴하지만 한번에 4바이트이니까 큰 오버헤드까지는 아니라고 생각함

	std::unordered_map<int, void (ClientSessionManager::* )(LPacket&)> RecvPacketFuncMap;//함수포인터 문법은 알다가도 모르겠다.

	std::vector<std::thread> ProcessRecvPacketThreads;

	std::mutex RecvPacketQueLock;//RecvQue 락
	std::condition_variable RecvPacketCV; // 생산자 소비자를 위한 CV;
};