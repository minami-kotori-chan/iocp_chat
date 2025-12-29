#pragma once
#include <basetsd.h>
#include <unordered_set>
#include <vector>
#include <shared_mutex>
#include "Packet.h"
#include "PacketSenderInterface.h"
#include "MonsterSpawner.h"

#define MAX_ENTER_USER_COUNT 1024
#define MAX_ROOM_COUNT 100

class ChatRoom
{
public:
	ChatRoom(std::atomic<UINT32>* ObjectIdCount)
	{
		EnterUsers.reserve(MAX_ENTER_USER_COUNT);
		const UINT8 SpawnerSize = 5;
		RoomMonsterSpawners.reserve(SpawnerSize);//일단 5개로 하고 나중에 랜덤으로 되게 하자

		for (int i = 0; i < SpawnerSize; i++) {
			RoomMonsterSpawners[i] = new MonsterSpawner();
			RoomMonsterSpawners[i]->SetObjectIdCounter(ObjectIdCount);
		}
	}

	bool EnterRoom(UINT32 UserIdx)
	{
		std::lock_guard<std::shared_mutex> lock(UserHashLock);
		if (EnterUsers.size() == MaxEnterSize) return false;
		EnterUsers.insert(UserIdx);
		return true;
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

private:

	UINT32 MaxEnterSize = MAX_ENTER_USER_COUNT;
	std::shared_mutex UserHashLock; 
	std::unordered_set<UINT32> EnterUsers;
	std::vector<MonsterSpawner*> RoomMonsterSpawners;
};

class RoomManager
{
public:
	void Init(UINT32 RoomsCount = MAX_ROOM_COUNT)
	{
		for (UINT32 i = 0; i < RoomsCount; i++) {
			Rooms.emplace_back(new ChatRoom(&ObjectIdCount));
		}
	}
	void SetSender(PacketSenderInterface* Sender)
	{
		MessageSender = Sender;
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
	void OnUpdateAllRoom(float DeltaTime)
	{
		for (auto* Room : Rooms) {
			Room->Update(MessageSender,DeltaTime);
		}
	}
private:


	PacketSenderInterface* MessageSender;
	std::vector<ChatRoom*> Rooms;
	std::atomic<UINT32> ObjectIdCount{ 0 };
};