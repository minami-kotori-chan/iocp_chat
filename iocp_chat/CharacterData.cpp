#include "CharacterData.h"
#include "MonsterSpawner.h"
#include "Packet.h"

void MonsterData::SetSpawner(MonsterSpawner* MSpawner)
{
	Spawner = MSpawner;
}

void MonsterData::OnDead()
{
	Spawner->OnMonsterDead(this);
}

void MonsterData::Update(float DeltaTime)//현재 상태에따라 순찰, 타켓이동을 위해 DestinationLocate으로 이동하는 코드 작성
{
	if (IsSpawned) { IsSpawned = false; return; }//이번 프레임에 스폰되었으면 return


}

void MonsterData::SetPacketData(MonsterMovingData& Packet)
{
	Packet.PacketId = PACKET_ID::MONSTER_DATA;
	Packet.PacketSize = sizeof(MonsterMovingData);
	Packet.State = State;
	Packet.UserId = ObjectId;//패킷구조를 유저이동동기화 패킷을 쓰다보니 userid라고 되어있는데 몬스터니까 objectid를 넣는다
	Packet.x = ActorPoint.x;
	Packet.y = ActorPoint.y;
	Packet.z= ActorPoint.z;
	Packet.Yaw =  ActorPoint.Yaw;
	Packet.Health = health;
	Packet.MonsterType = MonsterType;
}
