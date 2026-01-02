#pragma once
#include <basetsd.h>
#include <unordered_set>
#include <vector>
#include <shared_mutex>
#include <thread>
#include <condition_variable>
#include "Packet.h"
#include "PacketSenderInterface.h"
#include "MonsterSpawner.h"
#include "RoomJob.h"
#include "CharacterData.h"


#define MAX_ENTER_USER_COUNT 1024
#define MAX_ROOM_COUNT 100

class ChatRoom
{
public:
	ChatRoom(std::atomic<UINT32>* ObjectIdCount, RoomManager* RMptr)
	{
		EnterUsers.reserve(MAX_ENTER_USER_COUNT);
		const UINT8 SpawnerSize = 5;
		RoomMonsterSpawners.reserve(SpawnerSize);//일단 5개로 하고 나중에 랜덤으로 되게 하자
		roomManager = RMptr;
		
		for (int i = 0; i < SpawnerSize; i++) {
			RoomMonsterSpawners.push_back(new MonsterSpawner(ObjectIdCount));
			RoomMonsterSpawners[i]->InitRoomId(RoomId);
			RoomMonsterSpawners[i]->InitRoomManager(RMptr);
			RoomMonsterSpawners[i]->SetObjectIdToMPtr(ObjectIdToMonsterPtr);
		}
		InputJobQueue.reserve(1000);
		JobProcessQueue.reserve(1000);

		InitSection();
	}

	void InitSection()
	{
		float MapWidth = WorldMaxX - WorldMinX;
		float MapHeight = WorldMaxY - WorldMinY;

		SectionCountX = (UINT32)(MapWidth / SectionSize) + 1;
		SectionCountY = (UINT32)(MapHeight / SectionSize) + 1;

		GameSectionsOnlyMonster.resize(SectionCountX * SectionCountY);
		GameSectionsOnlyUser.resize(SectionCountX * SectionCountY);

		SectionActivate.resize(SectionCountX * SectionCountY,0);
	}
	// 좌표 섹션 변환
	UINT32 GetSectionIndex(float x, float y) {
		// 음수 양수 변환
		float OffsetX = x - WorldMinX;
		float OffsetY = y - WorldMinY;

		UINT32 IndX = (UINT32)(OffsetX / SectionSize);
		UINT32 IndY = (UINT32)(OffsetY / SectionSize);

		if (IndX >= SectionCountX) IndX = SectionCountX - 1;

		if (IndY >= SectionCountY) IndY = SectionCountY - 1;

		return (IndY * SectionCountX) + IndX;//섹션 번호
	}
	bool IsSectionActive(UINT32 SectionIdx)
	{
		return SectionActivate[SectionIdx];
	}
	bool EnterRoom(UINT32 UserIdx)
	{
		std::lock_guard<std::shared_mutex> lock(UserHashLock);
		if (EnterUsers.size() == MaxEnterSize) return false;
		EnterUsers.insert(UserIdx);
		return true;
	}
	bool EnterGameRoom(UINT32 UserIdx,Location& Locate)//이함수는 반드시 순차적(동시에 하나의 스레드만 접근해야함)
	{
		if (EnterUsers.size() == MaxEnterSize) return false;
		EnterUsers.insert(UserIdx);
		GameSectionsOnlyUser[GetSectionIndex(Locate.x, Locate.y)].push_back(UserIdx);
		return true;
	}
	bool EnterObjectInRoom(UINT32 ObjectId, Location& Locate)
	{
		GameSectionsOnlyMonster[GetSectionIndex(Locate.x, Locate.y)].push_back(ObjectId);
		return true;
	}
	bool LeaveGameRoom(UINT32 UserIdx, UINT32 SectionIdx)
	{
		if (EnterUsers.erase(UserIdx) > 0) {
			return LeaveUserSection(UserIdx, SectionIdx);
		}
		return false;
	}
	bool LeaveUserSection(UINT32 UserIdx, UINT32 SectionIdx)
	{
		UINT32 Section = SectionIdx;
		for (int i = 0; i < GameSectionsOnlyUser[Section].size(); i++) {
			if (GameSectionsOnlyUser[Section][i] == UserIdx)
			{
				GameSectionsOnlyUser[Section][i] = GameSectionsOnlyUser[Section][GameSectionsOnlyUser[Section].size() - 1];//맨 마지막 요소를 현재 요소로 교체
				GameSectionsOnlyUser[Section].pop_back();
				return true;
			}
		}
		return false;
	}
	bool LeaveObjectSection(UINT32 ObjectId, UINT32 SectionIdx)
	{
		UINT32 Section = SectionIdx;
		for (int i = 0; i < GameSectionsOnlyMonster[Section].size();i++) {
			if (GameSectionsOnlyMonster[Section][i] == ObjectId)
			{
				GameSectionsOnlyMonster[Section][i] = GameSectionsOnlyMonster[Section][GameSectionsOnlyMonster[Section].size() - 1];//맨 마지막 요소를 현재 요소로 교체
				GameSectionsOnlyMonster[Section].pop_back();
				return true;
			}
		}
		return false;
	}
	INT32 GetSectionX(float x)
	{
		float OffsetX = x - WorldMinX;
		if (OffsetX < 0) OffsetX = 0;
		INT32 IndX = (INT32)(OffsetX / SectionSize);

		if (IndX >= SectionCountX){
			IndX = SectionCountX - 1;
		}

		return IndX;
	}

	INT32 GetSectionY(float y)
	{
		float OffsetY = y - WorldMinY;
		if (OffsetY < 0) OffsetY = 0;
		INT32 IndY = (INT32)(OffsetY / SectionSize);

		if (IndY >= SectionCountY){
			IndY = SectionCountY - 1;
		}

		return IndY;
	}
	// 반환값: 가장 가까운 유저의 ID (못 찾으면 최대값반환) 유저 id만 가지고 좌표를 얻어야하는데 구조적으로 clientsession에 접근해야해서 cpp파일로 분리함(전방선언한 포인터를 써야해서)
	UINT32 FindNearestUserInSight(const Location& MonsterLoc, float MonsterYaw, float Range, float FovAngle);
	UINT32 FindNearestMonsterInSight(const Location& UserLoc, float UserYaw, float Range, float FovAngle);
	bool IsInSight(const Location& MonsterLoc, float MonsterYaw, const Location& TargetLoc, float Range, float FovAngle);
	Location GetUserLocation(UINT32 Userid);
	void OnUserDamage(UINT32 AttackObjectId,UINT32 DamagedUsertId,float Damage,UINT32 Section);

	void OnMonsterAttack(PacketSenderInterface* MessageSender,UINT32 ObjectId,UINT32 Section)
	{
		NoticeUserExit Pkt;//패킷구조는 방나가기와 동일하게 오브젝트번호만 있으면 됨
		Pkt.PacketId = PACKET_ID::MONSTER_ATTACK;
		Pkt.PacketSize = sizeof(NoticeUserExit);
		Pkt.UserId = ObjectId;
		LpPacket SendPkt;
		SendPkt.PacketSize = Pkt.PacketSize;
		SendPkt.pData = (char*) & Pkt;
		BroadCastAllRoomUser(MessageSender, SendPkt);
	}

	void OnMonsterDead(PacketSenderInterface* MessageSender, UINT32 ObjectId, UINT32 Section)
	{
		NoticeUserExit Pkt;//패킷구조는 방나가기와 동일하게 오브젝트번호만 있으면 됨
		Pkt.PacketId = PACKET_ID::NOTICE_MONSTER_DEAD;
		Pkt.PacketSize = sizeof(NoticeUserExit);
		Pkt.UserId = ObjectId;
		LpPacket SendPkt;
		SendPkt.PacketSize = Pkt.PacketSize;
		SendPkt.pData = (char*)&Pkt;
		BroadCastAllRoomUser(MessageSender, SendPkt);
	}

	void OnMonsterDamaged(UINT32 ObjectId,float Damage)
	{
		ObjectIdToMonsterPtr[ObjectId]->OnDamaged(Damage);
	}

	bool MoveUserInRoom(UINT32 ObjectId, UINT32 SectionIdx, const Location& locate)	//섹션이 변경되면 true
	{
		UINT32 NewSectionIdx = GetSectionIndex(locate.x, locate.y);

		if (SectionIdx == NewSectionIdx)
		{
			return false; // 변경 없음
		}

		LeaveObjectSection(ObjectId, SectionIdx);

		GameSectionsOnlyUser[NewSectionIdx].push_back(ObjectId);

		return true; // 섹션 변경됨
	}
	bool MoveObjectInRoom(UINT32 ObjectId, UINT32 SectionIdx,const Location& locate)	//섹션이 변경되면 true
	{
		UINT32 NewSectionIdx = GetSectionIndex(locate.x, locate.y);

		if (SectionIdx == NewSectionIdx)
		{
			return false; // 변경 없음
		}

		LeaveObjectSection(ObjectId, SectionIdx);

		GameSectionsOnlyMonster[NewSectionIdx].push_back(ObjectId);
		ObjectIdToMonsterPtr[ObjectId]->Section = NewSectionIdx;


		return true; // 섹션 변경됨
	}
	bool LeaveRoom(UINT32 UserIdx)
	{
		std::lock_guard<std::shared_mutex> lock(UserHashLock);
		if (EnterUsers.erase(UserIdx) > 0) return true;
		return false;
	}

	bool CheckUserInRoom(UINT32 UserIdx)
	{
		std::shared_lock<std::shared_mutex> lock(UserHashLock);
		if (EnterUsers.find(UserIdx) != EnterUsers.end()) return true;
		return false;
	}

	bool GetEnterRoomClientList(char* UserListArray,UINT32& UserSize)
	{
		UINT32* UserCopy = (UINT32*)UserListArray;
		{
			std::shared_lock<std::shared_mutex> lock(UserHashLock);
			int ArrayCount = 0;
			UserSize = EnterUsers.size();
			for (const auto& i : EnterUsers) {
				UserCopy[ArrayCount] = i;
				ArrayCount++;
			}
		}
		return true;
	}

	void BroadCastAllRoomUser(PacketSenderInterface* MessageSender,LPacket& pData)
	{
		/* 바로 send하는 동작방식
		std::shared_lock<std::shared_mutex> lock(UserHashLock);
		for (const auto& i : EnterUsers) {
			MessageSender->SendData(i,pData.pData, pData.PacketSize);//직접전송 resultque에 넣으면 대기시간 때문에 delay가능성때문에
		}
		*/
		// 해시를 복사해서 락 점유를 최소화 하는 동작방식
		UINT32 UserCopy[MAX_ENTER_USER_COUNT];
		UINT32 totaluser = 0;
		{
			std::shared_lock<std::shared_mutex> lock(UserHashLock);
			int ArrayCount=0;
			totaluser=EnterUsers.size();
			for(const auto& i : EnterUsers){
				UserCopy[ArrayCount] = i;
				ArrayCount++;
			}
		}
		/*
		for(const auto& i : UserCopy){
			MessageSender->SendData(i,pData.pData, pData.PacketSize);//직접전송
		}*/
		for (UINT32 i = 0; i < totaluser; i++) {
			MessageSender->SendData(UserCopy[i], pData.pData, pData.PacketSize);
			
		}
	}
	void BroadCastAllRoomUser(PacketSenderInterface* MessageSender, LpPacket& pData)
	{
		UINT32 UserCopy[MAX_ENTER_USER_COUNT];
		UINT32 totaluser = 0;
		GetEnterRoomClientList((char*)UserCopy, totaluser);
		/*
		for(const auto& i : UserCopy){
			MessageSender->SendData(i,pData.pData, pData.PacketSize);//직접전송
		}*/
		for (UINT32 i = 0; i < totaluser; i++) {
			MessageSender->SendData(UserCopy[i], pData.pData, pData.PacketSize);

		}
	}

	void Update(PacketSenderInterface* MessageSender,float DeltaTime)
	{
		if (EnterUsers.size() == 0) return;
		
		char PacketBuffer[1024];
		char* BufferTail = PacketBuffer;
		for (auto* Spawner : RoomMonsterSpawners) {
			Spawner->Update(DeltaTime);

			UINT16 SendIndex=0;
			UINT32 WriteBytes = 0;
			
			while (!(Spawner->SetMonstersPacket(BufferTail, PacketBuffer + sizeof(PacketBuffer), SendIndex, WriteBytes)))//모든 데이터를 날릴때까지 반복
			{
				BufferTail += WriteBytes;
				LpPacket packet;
				packet.PacketSize = BufferTail - PacketBuffer;
				packet.pData = PacketBuffer;
				BroadCastAllRoomUser(MessageSender, packet);
				BufferTail = PacketBuffer;
				WriteBytes = 0;
			}
			BufferTail += WriteBytes;
		}
		if (PacketBuffer != BufferTail) {
			LpPacket packet;
			packet.PacketSize = BufferTail - PacketBuffer;
			packet.pData = PacketBuffer;
			BroadCastAllRoomUser(MessageSender, packet);
			BufferTail = PacketBuffer;
		}
	}
	void PushJob(Job&& job)//큐에 넣은 스레드가 일까지 처리할지 말지를 bool로 반환
	{
		{
			std::lock_guard<std::mutex> lock(JobLock);
			InputJobQueue.push_back(job);
		}
	}
	bool TryGetAccess()
	{
		// 0에서 1 로 변경을 시도.
		return (RoomAccessGuard.exchange(1) == 0);//변경되었을때만 큐에넣자
	}
	void ProcessJobQueue()
	{
		while (true) //큐를 다 비울때까지 반복
		{
			{//큐 스왑
				std::lock_guard<std::mutex> lock(JobLock);

				if (InputJobQueue.empty())
				{
					// 처리할 잡이 없으면 0으로 변경
					RoomAccessGuard.store(0);
					return;
				}

				//스왑
				JobProcessQueue.swap(InputJobQueue);
			}

			// 잡 처리
			for (auto& job : JobProcessQueue)
			{
				job(); // 잡 실행
			}

			JobProcessQueue.clear();
		}
	}

	UINT32 RoomId;
private:

	UINT32 MaxEnterSize = MAX_ENTER_USER_COUNT;
	std::shared_mutex UserHashLock; 
	std::unordered_set<UINT32> EnterUsers;
	std::vector<MonsterSpawner*> RoomMonsterSpawners;

	std::vector<Job> InputJobQueue;
	std::vector<Job> JobProcessQueue;
	std::mutex JobLock;
	std::atomic<UINT8> RoomAccessGuard{ 0 };

	std::vector<std::vector<UINT32>> GameSectionsOnlyMonster;
	std::vector<std::vector<UINT32>> GameSectionsOnlyUser;
	std::vector<UINT8> SectionActivate;

	std::unordered_map<UINT32, MonsterData*> ObjectIdToMonsterPtr;//오브젝트 Id로 몬스터 접근하게 하는 해시

	float WorldMinX = -25000.0f; // 맵의 최소 X좌표
	float WorldMaxX = 25000.0f; // 맵의 최대 X좌표
	float WorldMinY = -25000.0f; // 맵의 최소 Y좌표 
	float WorldMaxY = 25000.0f; // 맵의 최대 Y좌표

	float SectionSize = 1000.0f; // 섹션 1칸의 크기

	int SectionCountX = 0; // 가로 섹션 개수
	int SectionCountY = 0; // 세로 섹션 개수

	class RoomManager* roomManager;
};

class RoomManager
{
public:
	void Init(UINT32 RoomsCount = MAX_ROOM_COUNT,UINT32 SetProcessThreadCount=1)
	{
		for (UINT32 i = 0; i < RoomsCount; i++) {
			Rooms.emplace_back(new ChatRoom(&ObjectIdCount,this));
			Rooms[i]->RoomId = i;
		}
		for (UINT32 i = 0; i < SetProcessThreadCount; i++) {
			RoomJobProcessThreads.emplace_back([this]() {ProcessJob(); });
		}
	}
	void SetSender(PacketSenderInterface* Sender)
	{
		MessageSender = Sender;
	}
	void SetClientSessionManager(class ClientSessionManager* SessionManager)
	{
		ClientSessionPtr = SessionManager;
	}
	bool EnterRoom(UINT32 UserIdx,UINT32 RoomId)
	{
		if (RoomId < Rooms.size()) {
			return Rooms[RoomId]->EnterRoom(UserIdx);
		}
		return false;
	}
	bool LeaveRoom(UINT32 UserIdx, UINT32 RoomId)
	{
		if (RoomId < Rooms.size()) {
			return Rooms[RoomId]->LeaveRoom(UserIdx);
		}
		return false;
	}
	bool CheckUserInRoom(UINT32 UserIdx, UINT32 RoomId)
	{
		if (RoomId < Rooms.size()) {
			return Rooms[RoomId]->CheckUserInRoom(UserIdx);
		}
		return false;
	}
	void BroadCastAllRoomUser(UINT32 RoomId, LPacket& pData)
	{
		if (RoomId < Rooms.size()) {
			Rooms[RoomId]->BroadCastAllRoomUser(MessageSender,pData);
			//printf("%d번방 송신 완료", RoomId);
		}
	}
	void BroadCastAllRoomUser(UINT32 RoomId, LpPacket& pData)
	{
		if (RoomId < Rooms.size()) {
			Rooms[RoomId]->BroadCastAllRoomUser(MessageSender, pData);
			//printf("%d번방 송신 완료", RoomId);
		}
	}
	void GetRoomUserList(UINT32 RoomId,char* UserList,UINT32& UserSize)
	{
		if (RoomId < Rooms.size()) {
			Rooms[RoomId]->GetEnterRoomClientList(UserList, UserSize);
		}
	}
	void OnMonsterAttack(UINT32 RoomId,UINT32 ObjectId,UINT32 Section)
	{
		if (RoomId < Rooms.size()) {
			Rooms[RoomId]->OnMonsterAttack(MessageSender, ObjectId, Section);
		}
	}
	void OnMonsterDead(UINT32 RoomId, UINT32 ObjectId, UINT32 Section)
	{
		if (RoomId < Rooms.size()) {
			Rooms[RoomId]->OnMonsterDead(MessageSender, ObjectId, Section);
		}
	}
	void OnUpdateAllRoom(float DeltaTime)
	{
		for (auto* Room : Rooms) {
			Room->Update(MessageSender,DeltaTime);
			Job updateJob = [Room, this, DeltaTime](){Room->Update(this->MessageSender, DeltaTime);};

			PushJob(Room->RoomId, std::move(updateJob));
		}

	}
	ChatRoom* GetRoomPtr(UINT32 Roomid)
	{
		return Rooms[Roomid];
	}
	~RoomManager()
	{
		StopThread();
	}
	void PushJob(UINT32 RoomId, Job&& job)
	{
		Rooms[RoomId]->PushJob(std::move(job));
		if (Rooms[RoomId]->TryGetAccess())
		{
			std::lock_guard<std::mutex> lock(JobQueueLock);
			JobQueue.push_back(RoomId);
			JobQueueLockCV.notify_one();
		}
	}
private:
	void StopThread()
	{
		ThreadsRun = false;
		for (auto& i : RoomJobProcessThreads) {
			if (i.joinable()) {
				i.join();
			}
		}
	}
	void ProcessJob()
	{
		while (ThreadsRun)
		{
			UINT32 targetRoomId = 0;

			{
				std::unique_lock<std::mutex> lock(JobQueueLock);
				
				JobQueueLockCV.wait(lock, [this] {return !JobQueue.empty() || !ThreadsRun;});

				if (!ThreadsRun) break;

				targetRoomId = JobQueue.front();
				JobQueue.pop_front();

			}

			Rooms[targetRoomId]->ProcessJobQueue();
			
		}
	}

	PacketSenderInterface* MessageSender;
	std::vector<ChatRoom*> Rooms;
	std::atomic<UINT32> ObjectIdCount{ 20000 };//최종 플레이어 id보다 크게 설정해서 겹치지않게 하자
	std::vector<std::thread> RoomJobProcessThreads;

	std::deque<UINT32> JobQueue;// 어떤 방의 작업을 처리해야하는지 적어놓는 큐
	std::mutex JobQueueLock;//위 큐의 락
	std::condition_variable JobQueueLockCV;

	bool ThreadsRun = true;

	friend class ChatRoom;

	class ClientSessionManager* ClientSessionPtr;
};