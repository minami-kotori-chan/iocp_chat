#include "CharacterData.h"
#include "MonsterSpawner.h"
#include "RoomManager.h"
#include "Packet.h"

void MonsterData::SetSpawner(MonsterSpawner* MSpawner)
{
	Spawner = MSpawner;
}

void MonsterData::OnDead()
{
	Spawner->OnMonsterDead(this);

}

void MonsterData::OnDamaged(float Damage)
{
    health -= Damage;
    if (health <= 0.f) {
        OnDead();
        Spawner->GetRoomManager()->OnMonsterDead(Spawner->GetRoomId(),ObjectId,Section);
    }
}

void MonsterData::Update(float DeltaTime)//현재 상태에따라 순찰, 타켓이동을 위해 DestinationLocate으로 이동하는 코드 작성
{//추가로 해당코드는 어차피 룸처리 스레드에서 호출됨(스레드 세이프함이 보장됨)
	if (IsSpawned) { IsSpawned = false; return; }//이번 프레임에 스폰되었으면 return
	//우선 근처에 플레이어가 있는지 보고
	//가장 가까운 플레이어를 타켓팅하고
	//상태를 추격으로 바꾸고
	//이동한다.
    /*
	if (TargetUser == 0xffffffff) {
		TargetUser = Spawner->GetRoomManager()->GetRoomPtr(Spawner->GetRoomId())->FindNearestUserInSight(ActorPoint, ActorPoint.Yaw, 500.f, 120.f);
		if (TargetUser == 0xffffffff) return;
	}
	else {
		//기존의 타겟팅 유저가 너무 먼지 확인 후 TargetUser초기화 할지 말지 결정
	}*/
	//Spawner->GetRoomManager()->GetRoomPtr(Spawner->GetRoomId())->GetUserLocation(TargetUser);//이 코드를 사용하면 유저의 location을 얻을 수 있음 반환 타입 : Location

    const UINT32 INVALID_USER_ID = 0xffffffff;
    const float ATTACK_RANGE = 100.0f;      // 공격 사거리
    const float GIVE_UP_RANGE = 800.0f;     // 추격 포기 거리
    const float TRACE_RANGE = 500.0f;       // 탐지 범위
    const float MOVE_SPEED = 100.0f;        // 몬스터 이동 속도
    const float ANGLE_RANGE = 120.0f;       // 시야각
    const float ATTACK_TIME = 3.f;
    const float ATTACK_DAMAGE = 1.f;

    
    auto CurrentRoom = Spawner->GetRoomManager()->GetRoomPtr(Spawner->GetRoomId());
    if (AttckCoolTimeProgress != 0.f) {
        AttckCoolTimeProgress -= DeltaTime;
        if (AttckCoolTimeProgress <= 0.f) AttckCoolTimeProgress = 0.f;
    }

    if (State == (UINT8)MonsterState::ATTACK) {
        AttackTime += DeltaTime;
        if (AttackTime < ATTACK_TIME) return;
        //범위 안에 타켓유저가 있는지 확인 후 체력깎기
        if (CurrentRoom->IsInSight(ActorPoint, ActorPoint.Yaw, CurrentRoom->GetUserLocation(TargetUser), ATTACK_RANGE, ANGLE_RANGE))//타켓이 범위 안에 있다면
        {
            CurrentRoom->OnUserDamage(ObjectId, TargetUser, ATTACK_DAMAGE, Section);
        }
        State = (UINT8)MonsterState::IDLE;
        AttackTime = 0;
    }

    if (TargetUser == INVALID_USER_ID)
    {
        TargetUser = CurrentRoom->FindNearestUserInSight(ActorPoint, ActorPoint.Yaw, TRACE_RANGE, ANGLE_RANGE);

        if (TargetUser == INVALID_USER_ID) return;
    }

    Location UserLoc = CurrentRoom->GetUserLocation(TargetUser);

    float dx = UserLoc.x - ActorPoint.x;
    float dy = UserLoc.y - ActorPoint.y;
    float DistSq = (dx * dx) + (dy * dy);

    //추격 포기
    if (DistSq > GIVE_UP_RANGE * GIVE_UP_RANGE)
    {
        TargetUser = INVALID_USER_ID; // 타겟 초기화
        State == (UINT8)MonsterState::IDLE;
        //원래 자리로 돌아가기
        return;
    }

    if (DistSq <= ATTACK_RANGE * ATTACK_RANGE)
    {
        // 이동 멈춤 + 공격 수행
        //공격이후 공격 쿨타임 로직 추가
        State = (UINT8)MonsterState::ATTACK;
        //실제로 플레이어의 체력을 깎는 판정은 다음프레임에도 사거리에 있는가를 확인후 해야함(우선은 패킷만 날림)
        Spawner->GetRoomManager()->OnMonsterAttack(Spawner->GetRoomId(), ObjectId, Section);//공격패킷발송
        //printf("몬스터의공격패킷발송\n");
        AttckCoolTimeProgress = AttackCoolTime;
        return;
    }
    else
    {
        // 이동
        float Dist = std::sqrt(DistSq);

        //방향 벡터
        float DirX = dx / Dist;
        float DirY = dy / Dist;

        // 위치 업데이트
        ActorPoint.x += DirX * MOVE_SPEED * DeltaTime;
        ActorPoint.y += DirY * MOVE_SPEED * DeltaTime;

        //몬스터가 타겟을 바라보게 함
        float Radian = std::atan2(DirY, DirX);
        ActorPoint.Yaw = Radian * (180.0f / 3.14);

        // 8. 룸에 위치 갱신 알림 (섹션 이동 처리 등을 위해 필수)
        // 몬스터 객체 내부의 Move 함수나, 룸의 MoveObject 함수 호출
        CurrentRoom->MoveObjectInRoom(ObjectId, Section, ActorPoint);
    }
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


