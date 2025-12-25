#pragma once

#ifndef MAX_PACKET_SIZE//헤더가 포함되지 않았으면 포함하기 위해서 packet헤더의 매크로를 이용
#include "Packet.h"//클라이언트세션에서 버퍼의 패킷을 꺼낼때 한개씩 꺼내기 위해서는 패킷구조를 사용해야하는데 이를 위해 헤더 포함(설계변경을 한번 해서 iocp코어에 packet헤더가 있는 형태에서 애플리케이션단으로 넘어온거라 ifndef로 포함하는중 추후 ifndef는 삭제예정)
#endif // !MAX_PACKET_SIZE
#include <deque>
#include <unordered_map>
#include <functional>
#include <string>
#include <shared_mutex>
#include "DelegateManager.h"
#include "ResultQueManager.h"
#include "RoomManager.h"
#include "PacketSenderInterface.h"
#include "CharacterData.h"

enum class ClinetState : UINT16
{
	NONE=0,
	CONNECTED=1000,//연결은 됐는데 로그인은 안된 상태
	LOGIN=2000,//로그인까지만 되어있는 상태
	ROOMIN=3000,//방 들어간 상태
};

#define MAX_SEESION_BUFSIZE MAX_SOCKBUF * 8//IOCP코어의 소켓크기 * 8로 링버퍼 초과 막기

struct ClientSession
{
	UINT32 ClientIdx;//클라이언트 ID
	ClinetState CState;
	CharacterData Cdata;

	char SessionRecvBuf[MAX_SEESION_BUFSIZE];

	UINT16 BufHead;//buf Read
	UINT16 BufTail;//buf Write
	UINT16 BufDataSize;

	UINT32 RoomId;

	std::string UserName;//추후에 이것도 char배열로 만들어야 함 동적할당 최소화를 위해서

	std::shared_mutex ClientSessionLock;//세션 락, 추후에 CSate를 아토믹으로 만들어 주자<<<바꾸려고 했는데 함수에서 CState만 바꾸는게 아니라서 그냥 뮤텍스 유지하기로함

	ClientSession()
	{
		CState = ClinetState::NONE;
		BufHead = 0;
		BufTail = 0;
		BufDataSize = 0;
		UserName.reserve(MAX_USERNAME_LENGTH);
		Cdata.SetRandomForTest();//랜덤 초기화
	}

	bool SetDataOnBuf(char *pData, UINT16 pDataSize)//락필요
	{
		std::lock_guard<std::shared_mutex> lock(ClientSessionLock);
		if (BufDataSize + pDataSize > MAX_SEESION_BUFSIZE) return false;

		// 현재 Tail 위치에서 버퍼 끝까지 남은 공간 계산
		int spaceAtEnd = MAX_SEESION_BUFSIZE - BufTail;

		if (pDataSize <= spaceAtEnd)
		{
			//뒤쪽 공간이 충분함
			CopyMemory(&SessionRecvBuf[BufTail], pData, pDataSize);

			BufTail += pDataSize;

			//Tail이 끝에 도달했으면 0으로
			if (BufTail == MAX_SEESION_BUFSIZE) BufTail = 0;
		}
		else
		{
			//뒤쪽 공간이 부족함 두 번 나누어 복사 (끝부분 + 앞부분)

			//뒷부분 채우기 (BufTail부터 버퍼 끝)
			CopyMemory(&SessionRecvBuf[BufTail], pData, spaceAtEnd);

			//남은 앞부분 채우기 (인덱스 0부터 나머지)
			int remainSize = pDataSize - spaceAtEnd;
			CopyMemory(&SessionRecvBuf[0], pData + spaceAtEnd, remainSize);

			//Tail 위치 갱신 (앞부분에 쓴 만큼 이동)
			BufTail = remainSize;
		}
		// 데이터 총량 증가
		BufDataSize += pDataSize;

		return true;
		/*
		if (BufTail + pDataSize >= MAX_SEESION_BUFSIZE)
		{
			if (BufDataSize != 0) {
				CopyMemory(&SessionRecvBuf[0], &SessionRecvBuf[BufHead], BufDataSize);//읽어야하는 부분부터 복제
				BufHead = 0;
				BufTail = BufDataSize;
			}
			else {
				BufTail = 0;
				BufHead = 0;
			}
		}
		CopyMemory(&SessionRecvBuf[BufTail], pData, pDataSize);
		BufTail += pDataSize;
		BufDataSize += pDataSize;
		return true;
		*/
	}

	void SetSystemDataOnBuf(PACKET_ID pId)//락필요
	{
		PacketHead pHead;
		pHead.PacketId = pId;
		pHead.PacketSize = sizeof(PacketHead);
		SetDataOnBuf((char*)&pHead, pHead.PacketSize);
	}

	LPacket GetDataOnBuf()//락필요
	{
		std::lock_guard<std::shared_mutex> lock(ClientSessionLock);
		//헤더 크기만큼 데이터가 있는지 확인
		if (BufDataSize < sizeof(PacketHead)) {
			return LPacket(); // 데이터 부족
		}
		PacketHead header;
		int spaceAtEnd = MAX_SEESION_BUFSIZE - BufHead; // 현재 Head에서 끝까지 남은 크기

		if (spaceAtEnd >= sizeof(PacketHead)) {
			// 헤더가 연속된 공간에 있으면 바로 복사
			CopyMemory(&header, &SessionRecvBuf[BufHead], sizeof(PacketHead));
		}
		else {
			// 헤더가 끊겨 있으면 조립해서 복사
			CopyMemory(&header, &SessionRecvBuf[BufHead], spaceAtEnd);
			CopyMemory((char*)&header + spaceAtEnd, &SessionRecvBuf[0], sizeof(PacketHead) - spaceAtEnd);
		}
		LPacket pInfo;
		pInfo.PacketId = header.PacketId;
		pInfo.PacketSize = header.PacketSize;
		pInfo.ClientIdx = ClientIdx;
		//pInfo.pData = &SessionRecvBuf[BufHead];//호환성을 위해 남겨둠
		// 데이터가 끊겨 있는지 확인
		if (spaceAtEnd >= header.PacketSize)
		{
			// CASE A: 데이터가 연속되어 있음 (한 번에 복사)
			CopyMemory(pInfo.pData, &SessionRecvBuf[BufHead], header.PacketSize);

			// Head 이동
			BufHead += header.PacketSize;
			// 정확히 끝에 도달했으면 0으로 (SetData와 동일한 로직)
			if (BufHead == MAX_SEESION_BUFSIZE) BufHead = 0;
		}
		else
		{
			// CASE B: 데이터가 끝과 처음에 나뉘어 있음 (두 번 복사)
			// 1) 뒷부분 복사
			CopyMemory(pInfo.pData, &SessionRecvBuf[BufHead], spaceAtEnd);
			// 2) 앞부분 복사
			CopyMemory(pInfo.pData + spaceAtEnd, &SessionRecvBuf[0], header.PacketSize - spaceAtEnd);

			// Head 이동 (앞부분만큼 이동한 위치가 됨)
			BufHead = header.PacketSize - spaceAtEnd;
		}

		// 6. 데이터 크기 차감
		BufDataSize -= header.PacketSize;
		
		return pInfo;
		/*
		std::lock_guard<std::shared_mutex> lock(ClientSessionLock);
		PacketHead* pHead = (PacketHead*)(&SessionRecvBuf[BufHead]);
		if (BufDataSize == 0 || pHead->PacketSize == 0) {
			return LPacket();
		}
		LPacket pInfo;
		pInfo.PacketId = pHead->PacketId;
		pInfo.PacketSize = pHead->PacketSize;
		pInfo.pData = &SessionRecvBuf[BufHead];
		pInfo.ClientIdx = ClientIdx;

		CopyMemory(&pInfo.PData, &SessionRecvBuf[BufHead],pHead->PacketSize);

		BufHead += pHead->PacketSize;
		BufDataSize -= pHead->PacketSize;
		return pInfo;
		*/
	}

	UINT32 GetUserRoomId()//읽기만 하니까 sharedlock으로
	{
		std::shared_lock<std::shared_mutex> lock(ClientSessionLock);
		return RoomId;
	}
	void OnConnect()
	{
		{
			std::lock_guard<std::shared_mutex> lock(ClientSessionLock);
			CState = ClinetState::CONNECTED;
		}
		printf("\nClient Connected id : %d\n", ClientIdx);
	}
	void OnLogin(char *LoginName,UINT8 NameSize)//로그인 성공시
	{
		std::lock_guard<std::shared_mutex> lock(ClientSessionLock);
		UserName = LoginName;
		CState = ClinetState::LOGIN;
	}
	void OnLogout()
	{
		std::lock_guard<std::shared_mutex> lock(ClientSessionLock);
		CState = ClinetState::CONNECTED;
		UserName = "";
	}
	void EnterRoom(UINT32 Roomid)
	{
		std::lock_guard<std::shared_mutex> lock(ClientSessionLock);
		this->RoomId = Roomid;
		CState = ClinetState::ROOMIN;
	}
	void ExitRoom()
	{
		std::lock_guard<std::shared_mutex> lock(ClientSessionLock);
		CState = ClinetState::LOGIN;
	}
	void GetLocation(PacketMoveReq& Locate)
	{
		std::lock_guard<std::shared_mutex> lock(ClientSessionLock);
		Locate.x = Cdata.x;
		Locate.y = Cdata.y;
		Locate.z = Cdata.z;
		Locate.Yaw = Cdata.Yaw;
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

	void SetSender(PacketSenderInterface* sender)
	{
		//room 초기화
		roomManager.Init();
		roomManager.SetSender(sender);
		this->sender = sender;
	}
	void SetThroughput(std::atomic<UINT64>& lostpacket)
	{
		LostPacketCountPtr = &lostpacket;
	}
	ClientSession* GetClient(UINT32 idx)
	{
		return ClientSessions[idx];
	}

	void PushRecvPacket(UINT32 idx, char* pData, UINT16 pDataSize)
	{
		std::lock_guard<std::mutex> lock(RecvPacketQueLock);
		if (ClientSessions[idx]->SetDataOnBuf(pData, pDataSize)) {
			RecvPacketQueue.push_back(idx);
			RecvPacketCV.notify_one();
		}
		else {
			if (LostPacketCountPtr) {
				(*LostPacketCountPtr)++;
			}
		}
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
	void SetDelegate(DelegateManager<void, LPacket&>* pDM)
	{
		pDelegateManager = pDM;
	}
	void OnLoginSuccess(UINT32 idx, char* UserName, char NameSize)
	{
		UserName[NameSize - 1] = 0;
		ClientSessions[idx]->OnLogin(UserName, NameSize);
	}
	void BindResultQue(ResultQueManager* RManager)
	{
		RQueManager = RManager;
	}
private:
	void CreateProcessThreads(UINT32 ThreadCnt=14)
	{
		for (UINT32 i = 0; i < ThreadCnt; i++) {
			ProcessRecvPacketThreads.emplace_back([this]() {ProcessRecvPacket(); });
		}
	}

	void BindFunc()//함수포인터 바인딩
	{
		RecvPacketFuncMap[(int)PACKET_ID::CONNECT_REQUEST] = &ClientSessionManager::OnConnect;//멤버함수포인터는 &가 필수임
		RecvPacketFuncMap[(int)PACKET_ID::LOGIN_REQUEST] = &ClientSessionManager::OnLogin;
		RecvPacketFuncMap[(int)PACKET_ID::ECHO_MESSAGE] = &ClientSessionManager::OnEchoMessage;
		RecvPacketFuncMap[(int)PACKET_ID::LOGOUT_REQUEST] = &ClientSessionManager::OnLogout;
		RecvPacketFuncMap[(int)PACKET_ID::MESSAGE_REQUEST] = &ClientSessionManager::OnMessage;
		RecvPacketFuncMap[(int)PACKET_ID::ENTER_ROOM_REQUEST] = &ClientSessionManager::OnEnterRoom;
		RecvPacketFuncMap[(int)PACKET_ID::EXIT_ROOM_REQUEST] = &ClientSessionManager::OnExitRoom;
		RecvPacketFuncMap[(int)PACKET_ID::GUEST_REQUEST] = &ClientSessionManager::OnGuestLogin;
		RecvPacketFuncMap[(int)PACKET_ID::GAME_ROOM_ENTER_REQUEST] = &ClientSessionManager::OnGameRoomEnter;
		
	}

	void ProcessRecvPacket()//패킷처리 스레드에서 호출하는 함수
	{
		/*while (RecvPacketThreadRun)
		{
			LPacket packet;
			bool IsValid = false;
			{
				std::unique_lock<std::mutex> lock(RecvPacketQueLock);

				RecvPacketCV.wait(lock, [this] { return !RecvPacketQueue.empty() || !RecvPacketThreadRun; });
				
				if (RecvPacketThreadRun == false) break;

				if (!RecvPacketQueue.empty()){
					//packet = PopRecvPacket();
					IsValid = true;
				}

			}
			if(IsValid) PacketProcess(packet);
		}*/
		while (RecvPacketThreadRun)
		{
			LPacket packet = [this]() -> LPacket {
				std::unique_lock<std::mutex> lock(RecvPacketQueLock);

				RecvPacketCV.wait(lock, [this] { return !RecvPacketQueue.empty() || !RecvPacketThreadRun; });

				if (!RecvPacketThreadRun || RecvPacketQueue.empty()) {
					return LPacket();
				}

				return PopRecvPacket();
			}(); //RVO를 위해 람다로 작성
			PacketProcess(packet);
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
			printf("수신한 식별 불가능한 패킷 ID : %d\n", packet.PacketId);
			if (LostPacketCountPtr)
			{
				(*LostPacketCountPtr)++;
			}
		}
	}

	void OnEchoMessage(LPacket& packet)
	{
		packet.pData[packet.PacketSize] = 0;
		printf("수신 문자열 : %s", &(packet.pData[5]));
	}
	void OnConnect(LPacket& packet)
	{
		ClientSessions[packet.ClientIdx]->OnConnect();
	}

	void OnLogin(LPacket& packet)
	{
		LoginPacket* LoginP = (LoginPacket*)(packet.pData);
		//여기에 db요청 코드 필요함 아래 코드도 로그인 완료 이후에 동작하게 바꾸어야함 << 델리게이트를 사용하자! 델리게이트 이벤트 호출
		pDelegateManager->CallAllFunc(packet);//db에 요청보내는 경우 
		//resultque에 db에서 넣어주므로 que에 넣는 코드 작성할 필요 없음
		//ClientSessions[packet.ClientIdx]->OnLogin(LoginP->UserName, MAX_USERNAME_LENGTH);
	}
	void OnGuestLogin(LPacket& packet)
	{
		GuestPacket* LoginP = (GuestPacket*)(packet.pData);
		ClientSessions[packet.ClientIdx]->OnLogin(LoginP->UserName, LoginP->NameSize);
		printf("수신 문자열 : %s\n", &LoginP->UserName);
		//PushLpacketResult(PACKET_ID::GUEST_RESPONSE, true, packet);

		LoginResult loginResult;
		loginResult.PacketId = PACKET_ID::GUEST_RESPONSE;
		loginResult.Success = true;
		loginResult.UserId = packet.ClientIdx;
		CopyMemory(loginResult.UserName, LoginP->UserName, LoginP->NameSize);
		loginResult.NameSize = LoginP->NameSize;
		loginResult.PacketSize = sizeof(LoginResult)- sizeof(loginResult.UserName) + loginResult.NameSize;

		SendData(loginResult.UserId, (char*)&loginResult, loginResult.PacketSize);
	}
	void OnLogout(LPacket& packet)
	{
		ClientSessions[packet.ClientIdx]->OnLogout();
		PushLpacketResult(PACKET_ID::LOGIN_RESPONSE, true, packet);
	}
	void OnMessage(LPacket& packet)
	{

		roomManager.BroadCastAllRoomUser(ClientSessions[packet.ClientIdx]->GetUserRoomId(), packet);
		//룸에서 자체적으로 send하므로 큐에 넣을 필요 없음
	}
	void OnEnterRoom(LPacket& packet)
	{
		EnterRoomPacket* EnterPacket= (EnterRoomPacket*)packet.pData;
		bool Success = roomManager.EnterRoom(packet.ClientIdx, EnterPacket->RoomId);
		ClientSessions[packet.ClientIdx]->EnterRoom(EnterPacket->RoomId);
		PushLpacketResult(PACKET_ID::ENTER_ROOM_RESPONSE, Success,packet);
		if (Success == false) return;
		NoticeNewUserEnter NewUserEnter;
		NewUserEnter.PacketId = PACKET_ID::NOTICE_ROOM_NEW_USER;
		NewUserEnter.PacketSize = sizeof(NoticeNewUserEnter);
		CopyMemory(NewUserEnter.UserName, ClientSessions[packet.ClientIdx]->UserName.c_str(), ClientSessions[packet.ClientIdx]->UserName.size());
		LPacket SendPacket;
		SendPacket.PacketId = NewUserEnter.PacketId;
		SendPacket.PacketSize = NewUserEnter.PacketSize;
		SendPacket.ClientIdx = packet.ClientIdx;
		//SendPacket.pData = (char*) &NewUserEnter;
		CopyMemory(SendPacket.pData, &NewUserEnter,sizeof(NewUserEnter));
		roomManager.BroadCastAllRoomUser(ClientSessions[SendPacket.ClientIdx]->GetUserRoomId(), SendPacket);

		
	}
	void OnExitRoom(LPacket& packet)
	{ 
		bool Success = roomManager.LeaveRoom(packet.ClientIdx, ClientSessions[packet.ClientIdx]->GetUserRoomId());
		if (Success == false) return;
		ClientSessions[packet.ClientIdx]->ExitRoom();
		PushLpacketResult(PACKET_ID::EXIT_ROOM_RESPONSE,true, packet);//룸에 자기자신이 없기때문에 알려야함

		NoticeNewUserEnter NewUserEnter;
		NewUserEnter.PacketId = PACKET_ID::NOTICE_ROOM_EXIT_USER;
		NewUserEnter.PacketSize = sizeof(NoticeNewUserEnter);
		CopyMemory(NewUserEnter.UserName, ClientSessions[packet.ClientIdx]->UserName.c_str(), ClientSessions[packet.ClientIdx]->UserName.size());
		LPacket SendPacket;
		SendPacket.PacketId = NewUserEnter.PacketId;
		SendPacket.PacketSize = NewUserEnter.PacketSize;
		SendPacket.ClientIdx = packet.ClientIdx;
		//SendPacket.pData = (char*)&NewUserEnter;
		CopyMemory(SendPacket.pData, &NewUserEnter, sizeof(NewUserEnter));
		roomManager.BroadCastAllRoomUser(ClientSessions[SendPacket.ClientIdx]->GetUserRoomId(), SendPacket);
	}

	void OnGameRoomEnter(LPacket& packet)
	{
		EnterRoomPacket* EnterPacket = (EnterRoomPacket*)packet.pData;
		bool Success = roomManager.EnterRoom(packet.ClientIdx, EnterPacket->RoomId);
		ClientSessions[packet.ClientIdx]->EnterRoom(EnterPacket->RoomId);
		PushLpacketResult(PACKET_ID::ENTER_ROOM_RESPONSE, Success, packet);
		if (Success == false) return;
		NoticeNewUserEnterGame NewUserEnter;
		NewUserEnter.PacketId = PACKET_ID::NOTICE_ROOM_NEW_USER;
		NewUserEnter.PacketSize = sizeof(NoticeNewUserEnter);
		CopyMemory(NewUserEnter.UserName, ClientSessions[packet.ClientIdx]->UserName.c_str(), ClientSessions[packet.ClientIdx]->UserName.size());
		NewUserEnter.UserId = packet.ClientIdx;
		ClientSessions[packet.ClientIdx]->GetLocation(NewUserEnter.PlayerLocate);
		
		LPacket SendPacket;
		SendPacket.PacketId = NewUserEnter.PacketId;
		SendPacket.PacketSize = NewUserEnter.PacketSize;
		SendPacket.ClientIdx = packet.ClientIdx;
		//SendPacket.pData = (char*) &NewUserEnter;
		CopyMemory(SendPacket.pData, &NewUserEnter, sizeof(NewUserEnter));

		EnterRoomPacketResponse ResponsePacket;
		ResponsePacket.PacketId = PACKET_ID::GAME_ROOM_ENTER_RESPONSE;
		ResponsePacket.PacketSize = sizeof(EnterRoomPacketResponse);
		ResponsePacket.RoomId = EnterPacket->RoomId;
		ResponsePacket.UserId = packet.ClientIdx;
		SendData(ResponsePacket.UserId,(char*) & ResponsePacket, ResponsePacket.PacketSize);//방입장 응답 패킷

		roomManager.BroadCastAllRoomUser(ClientSessions[SendPacket.ClientIdx]->GetUserRoomId(), SendPacket);
	}
	void OnGameRoomExit(LPacket& packet)
	{

	}
	
	void PushLpacketResult(PACKET_ID pid,bool Success, LPacket& packet)
	{
		LPacketResult Rpacket;
		Rpacket.PacketId = pid;
		Rpacket.PacketSize = sizeof(ResponsePacket);
		Rpacket.Success = Success;
		Rpacket.ClientIdx = packet.ClientIdx;
		RQueManager->PushResultQue(Rpacket);
	}
	

	LPacket PopRecvPacket()
	{
		UINT32 idx = RecvPacketQueue.front();
		RecvPacketQueue.pop_front();
		return ClientSessions[idx]->GetDataOnBuf();
	}

	__inline void SendData(UINT32 idx, char* pData, int Psize)
	{
		if (sender){
			sender->SendData(idx, pData, Psize);
		}
	}

	bool RecvPacketThreadRun = true;
	std::vector<ClientSession*> ClientSessions;

	std::deque<UINT32> RecvPacketQueue;//동적할당이라서 불리하긴하지만 한번에 4바이트이니까 큰 오버헤드까지는 아니라고 생각함

	std::unordered_map<int, void (ClientSessionManager::* )(LPacket&)> RecvPacketFuncMap;//함수포인터 문법은 알다가도 모르겠다.

	std::vector<std::thread> ProcessRecvPacketThreads;

	std::mutex RecvPacketQueLock;//RecvQue 락
	std::condition_variable RecvPacketCV; // 생산자 소비자를 위한 CV;

	ResultQueManager* RQueManager=nullptr;

	DelegateManager<void, LPacket&>* pDelegateManager;

	RoomManager roomManager;
	std::atomic<UINT64>* LostPacketCountPtr=nullptr;
	std::atomic<UINT64>* Throughput=nullptr;

	PacketSenderInterface* sender=nullptr;
};