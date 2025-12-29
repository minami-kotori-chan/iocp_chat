#pragma once
#include <basetsd.h>
#include <atomic>
#include <deque>
#include "CharacterData.h"
#include <unordered_map>

struct SpawnData {
    UINT32 MonsterType; // 몬스터타입
    float CenterX, CenterY; // 스폰 중심 좌표
    float Radius;      // 퍼져있을 반경
    UINT32 MaxCount;    // 유지할 개체 수
    float RespawnTime; // 죽은 뒤 부활까지 걸리는 시간
};

class MonsterSpawner {
public:
    MonsterSpawner();
    MonsterSpawner(SpawnData InData);

    // 일정 시간마다 호출되어 리스폰 체크
    void Update(float DeltaTime);

    // 몬스터가 죽었을 때 호출됨
    void OnMonsterDead(MonsterData* Monster);
    void SetSpawnDataRandom();
    void SetObjectIdCounter(std::atomic<UINT32>* OCounter) { ObjectIdCounter = OCounter; }

    //버퍼에 패킷을 담아주는데 만약 다 못담은 경우 false 반환 추가로 SendStartIndex를 마지막으로 쓴 위치값으로 수정해줌
    bool SetMonstersPacket(const char* BufferHead, const char* LastBufferAddress, UINT16& SendStartIndex, UINT32& WriteBytes);

    //혹시 외부에서 몬스터에 접근해야하는 상황을 위해서 iterator 구현
    //using iterator = std::vector<MonsterData*>;
    auto begin() { return AliveMonsterArray.begin(); }
    auto end() { return AliveMonsterArray.end(); }
private:
    // 스폰 위치 계산 (원형 범위 내 랜덤)
    void GetRandomSpawnPos(float& OutX, float& OutY);
    void SpawnMonster();
    void InitializePool();
private:
    SpawnData Data;

    // 현재 살아있는 몬스터 수
    std::atomic<UINT32> CurrentCount{0};

    // 리스폰 대기열 (죽은 시간 + 리스폰 시간)

    // 현재 서버 시간 (누적)
    float ServerTime = 0.0f;

    std::deque<std::pair<float,UINT32>> RespawnQueue;
    // 실제 몬스터 데이터가 존재
    std::vector<MonsterData> Pool;
    // 가용 풀
    std::vector<int> EmptyMonster;
    // 살아있는 monster들 검색용
    std::unordered_map<MonsterData*, UINT16> AliveMonster;
    //살아있는 몬스터 접근용(패킷 포장할때)
    std::vector<MonsterData*> AliveMonsterArray;

    std::atomic<UINT32>* ObjectIdCounter;
};