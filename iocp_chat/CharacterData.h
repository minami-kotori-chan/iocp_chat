#pragma once
#include <basetsd.h>
#include <random>

struct Location {
    float x = 0;
    float y = 0;
    float z = 0;
    float Yaw = 0;
};

struct CharacterData
{
    Location ActorPoint;

	UINT32 health=100;

    UINT8 State=0;

    CharacterData()
    {
        SetRandomForTest();
    }
    CharacterData(UINT32 Dummy)
    {

    }
    void SetRandomForTest()
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(1.0f, 100.0f);

        ActorPoint.x = dist(gen);
        ActorPoint.y = dist(gen);
        ActorPoint.z = dist(gen);
        ActorPoint.Yaw = dist(gen);

        // health는 건드리지 않음
    }

};

enum class MonsterState : UINT8 {
    IDLE = 0,
    PATROLL = 1,
    WALK = 2,
    SPAWNED=1000,
};

class MonsterSpawner;//순환참조 막기용
struct PacketMoveRes;

struct MonsterData : CharacterData
{
    UINT32 MonsterType;
    MonsterSpawner* Spawner;
    UINT32 ObjectId;

    Location DestinationLocate;//타켓이 생겼을때 목표하는 지점 혹은 순찰중일때 타겟지점
    bool IsSpawned = true;
    
    void SetSpawner(MonsterSpawner* MSpawner);
    void SetMonsterData(UINT32 MType)
    {
        MonsterType = MType;
    }
    // 몬스터 초기화 (스폰될 때 호출)
    void Reset(float x, float y) {
        ActorPoint.x = x;
        ActorPoint.y = y;
        health = 100;
        State = 1000; // IDLE/WALK/PATROLL
        IsSpawned = true;
    }
    void OnDead();

    // 몬스터 AI/이동 로직 (매 프레임 호출)
    void Update(float DeltaTime);

    void SetPacketData(PacketMoveRes& Packet);
};