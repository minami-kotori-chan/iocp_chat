#include "MonsterSpawner.h"
#include <cmath>
#include <random>
#include "Packet.h"


MonsterSpawner::MonsterSpawner()
{
    SetSpawnDataRandom();
    InitializePool();
}

MonsterSpawner::MonsterSpawner(std::atomic<UINT32>* Counter) : ObjectIdCounter(Counter)
{
    SetSpawnDataRandom();
    InitializePool();
}

MonsterSpawner::MonsterSpawner(SpawnData InData): Data(InData)
{
    InitializePool();
}

void MonsterSpawner::InitializePool()
{
    
    Pool.resize(Data.MaxCount);
    
    EmptyMonster.reserve(Data.MaxCount);
    for (int i = 0; i < Data.MaxCount; ++i) {
        // 초기에는 모든 인덱스가 사용 가능
        EmptyMonster.push_back(i);
    
        // 역참조를 위해 스포너 포인터 미리 세팅
        Pool[i].SetSpawner(this);
        Pool[i].SetMonsterData(Data.MonsterType);

        Pool[i].ObjectId = ObjectIdCounter->fetch_add(1);//
    }
}

void MonsterSpawner::Update(float DeltaTime)
{
    ServerTime += DeltaTime; 

    // 초기 스폰 (부족한 만큼 채우기)
    while (CurrentCount < Data.MaxCount && RespawnQueue.empty()) {
        SpawnMonster();
    }

    for (auto monster : AliveMonster) {
        (monster.first)->Update(DeltaTime);
    }
    // 리스폰 대기열 처리
    while (!RespawnQueue.empty()) {
        if (RespawnQueue.front().first <= ServerTime) {
            SpawnMonster();

            // 큐에서 제거
            RespawnQueue.erase(RespawnQueue.begin());
        }
        else {
            break;
        }
    }
    
}

void MonsterSpawner::OnMonsterDead(MonsterData* Monster)
{
    if (AliveMonster.find(Monster) == AliveMonster.end()) return;//못찾으면 return
    AliveMonsterArray[AliveMonster[Monster]] = AliveMonsterArray[AliveMonsterArray.size() - 1];//맨마지막 요소로 교체
    AliveMonster[AliveMonsterArray[AliveMonsterArray.size() - 1]] = AliveMonster[Monster];//해시에 저장된 인덱스값 교체
    AliveMonsterArray.pop_back();//vector에서 삭제
    AliveMonster.erase(Monster);//해시에서 삭제
    // 풀 반납 (인덱스 계산)
    // 포인터 주소 차이 이용
    int Index = static_cast<int>(Monster - &Pool[0]);
    EmptyMonster.push_back(Index);

    RespawnQueue.push_back({ ServerTime + Data.RespawnTime,Index });
    CurrentCount--;//개체수 줄이기

}

void MonsterSpawner::SetSpawnDataRandom()
{
    // 난수 생성기
    static thread_local std::mt19937 Generator(std::random_device{}());

    // ==========================================
    const float MinRadis = -5000.0f;
    const float MaxRadis = 5000.0F;

    // 스폰 반경 (200 ~ 500)
    std::uniform_real_distribution<float> DistRadius(1000.0f, 3000.0f);

    // 위치
    std::uniform_real_distribution<float> DistPos(MinRadis, MaxRadis);

    // 개체 수 (3 ~ 10 마리)
    std::uniform_int_distribution<UINT32> DistCount(3, 10);

    // 리스폰 시간 (5초 ~ 15초)
    std::uniform_real_distribution<float> DistTime(5.0f, 15.0f);

    // 몬스터 타입 (1번 ~ 3번 몬스터)
    //std::uniform_int_distribution<UINT32> DistType(1, 3);
    // ==========================================

    // 데이터 대입
    //Data.MonsterType = DistType(Generator);
    Data.MonsterType = 0;
    Data.CenterX = DistPos(Generator);
    Data.CenterY = DistPos(Generator);
    Data.Radius = DistRadius(Generator);
    Data.MaxCount = DistCount(Generator);
    Data.RespawnTime = DistTime(Generator);

    // 기존에 데이터가 있었다면 초기화
    RespawnQueue.clear();
    RespawnQueue.resize(Data.MaxCount);
}

bool MonsterSpawner::SetMonstersPacket(const char* BufferHead,const char* LastBufferAddress, UINT16& SendStartIndex,UINT32& WriteBytes)
{
    UINT16 LastSend = 0;
    char* BufferWrite = (char*)BufferHead;
    for (auto i = AliveMonsterArray.begin()+SendStartIndex; i < AliveMonsterArray.end(); ++i, LastSend++) {
        if (BufferWrite + sizeof(MonsterMovingData) > LastBufferAddress) { SendStartIndex += LastSend; return false; }//쓸 공간이 없으면 false반환
        MonsterMovingData MonsterPacket;//원래는 player용으로 설계된 패킷구조이긴하나 몬스터를 담아도 크게 문제없을듯함
        (*i)->SetPacketData(MonsterPacket);
        memcpy(BufferWrite,&MonsterPacket, MonsterPacket.PacketSize);
        LastSend++;
        BufferWrite += sizeof(MonsterMovingData);
        WriteBytes += sizeof(MonsterMovingData);
    }
    
    
    return true;
}

void MonsterSpawner::GetRandomSpawnPos(float& OutX, float& OutY)
{
    static thread_local std::mt19937 Generator(std::random_device{}());

    std::uniform_real_distribution<float> AngleDist(0.0f, 360.f);

    // 랜덤 거리 생성
    std::uniform_real_distribution<float> RangeDist(0.0f, Data.Radius);

    // 난수 추출
    float Angle = AngleDist(Generator);
    float Distance = RangeDist(Generator);


    // 극좌표계 -> 직교좌표계 변환
    OutX = Data.CenterX + (std::cos(Angle) * Distance);
    OutY = Data.CenterY + (std::sin(Angle) * Distance);
}

void MonsterSpawner::SpawnMonster()
{
    // Room->CreateMonster(Data.MonsterType, x, y);
    // CurrentCount++; // 성공 시 증가
    // 풀에서 꺼내기
    int Index = EmptyMonster.back();
    EmptyMonster.pop_back();

    MonsterData* Monster = &Pool[Index];

    // 위치 및 상태 리셋
    float x, y;
    GetRandomSpawnPos(x, y);
    Monster->Reset(x, y);

    // 리스트에 등록
    AliveMonsterArray.push_back(Monster);
    AliveMonster.insert({ Monster,AliveMonsterArray.size() - 1 });

    CurrentCount++; 
}