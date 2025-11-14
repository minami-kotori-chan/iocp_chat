#pragma once
#include <basetsd.h>
#include <unordered_set>
#include <vector>
#include <shared_mutex>
#include "Packet.h"
#include "PacketSenderInterface.h"

#define MAX_ENTER_USER_COUNT 1024

class ChatRoom
{
public:
	ChatRoom()
	{
		EnterUsers.reserve(MAX_ENTER_USER_COUNT);
	}

	bool EnterRoom(UINT32 UserIdx)
	{
		std::lock_guard<std::shared_mutex> lock(UserHashLock);
		if (EnterUsers.size() == MaxEnterSize) return false;
		EnterUsers.insert(UserIdx);
	}

	void LeaveRoom(UINT32 UserIdx)
	{
		std::lock_guard<std::shared_mutex> lock(UserHashLock);
		EnterUsers.erase(UserIdx);
	}

	bool CheckUserInRoom(UINT32 UserIdx)
	{
		std::shared_lock<std::shared_mutex> lock(UserHashLock);
		if (EnterUsers.find(UserIdx) != EnterUsers.end()) return true;
		return false;
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
		char UserCopy[MAX_ENTER_USER_COUNT];
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
		for(const auto& i : UserCopy){
			MessageSender->SendData(i,pData.pData, pData.PacketSize);//직접전송
		}
		
	}

private:

	UINT32 MaxEnterSize = MAX_ENTER_USER_COUNT;
	std::shared_mutex UserHashLock;
	std::unordered_set<UINT32> EnterUsers;
};

class RoomManager
{
public:
	void Init(UINT32 RoomsCount=100)
	{
		for (UINT32 i = 0; i < RoomsCount; i++) {
			Rooms.emplace_back(new ChatRoom());
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
	void LeaveRoom(UINT32 UserIdx, UINT32 RoomId)
	{
		if (RoomId < Rooms.size()) {
			Rooms[RoomId]->LeaveRoom(UserIdx);
		}
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
		}
	}
private:


	PacketSenderInterface* MessageSender;
	std::vector<ChatRoom*> Rooms;
};