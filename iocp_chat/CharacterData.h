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

	float health=100.f;

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
    SPAWNED=200,
    ATTACK=100,
};

class MonsterSpawner;//순환참조 막기용
struct MonsterMovingData;

struct MonsterData : public CharacterData
{
    UINT32 MonsterType;
    MonsterSpawner* Spawner;
    UINT32 ObjectId;
    UINT32 Section;
    UINT32 TargetUser= 0xffffffff;
    UINT32 IndexInSpawner;

    float AttackTime=0.f;//공격모션 진행시간(일정시간 이후에 실제 공격 판정을 내림)
    const float AttackCoolTime=1.0f;//공격 이후에 쿨타임
    float AttckCoolTimeProgress = 0.f;//쿨타임 남은 시간

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
        State = 200; // IDLE/WALK/PATROLL
        IsSpawned = true;

    }
    
    void OnDead();

    // 몬스터 AI/이동 로직 (매 프레임 호출)
    void Update(float DeltaTime);

    void SetPacketData(MonsterMovingData& Packet);
    void OnDamaged(float Damage);
};