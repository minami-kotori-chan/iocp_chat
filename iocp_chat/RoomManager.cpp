#include "RoomManager.h"
#include "IOCP/Iocp.h"
#include "ClientSession.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14
#endif
#define TO_RAD (M_PI / 180.0f)

UINT32 ChatRoom::FindNearestUserInSight(const Location& MonsterLoc, float MonsterYaw, float Range, float FovAngle)
{
	UINT32 BestTargetId = 0xffffffff;
	float MinDistSq = Range * Range; // 탐색 범위의 제곱을 초기 최소 거리로 설정

	// 섹션 인덱스 계산
	int CenterX = GetSectionX(MonsterLoc.x);
	int CenterY = GetSectionY(MonsterLoc.y);

	// 주변 9개 섹션 순회
	for (int y = -1; y <= 1; ++y)
	{
		for (int x = -1; x <= 1; ++x)
		{
			int CheckX = CenterX + x;
			int CheckY = CenterY + y;

			if (CheckX < 0 || CheckX >= SectionCountX || CheckY < 0 || CheckY >= SectionCountY)
				continue;

			int SectionIdx = (CheckY * SectionCountX) + CheckX;


			// 유저 순회
			for (UINT32 UserIdx : GameSectionsOnlyUser[SectionIdx])
			{
				
				Location UserLoc = roomManager->ClientSessionPtr->GetClient(UserIdx)->GetLocate();
				
				float dx = UserLoc.x - MonsterLoc.x;
				float dy = UserLoc.y - MonsterLoc.y;
				float DistSq = (dx * dx) + (dy * dy);

				if (DistSq >= MinDistSq) continue;

				// 시야각 체크
				
				if (IsInSight(MonsterLoc, MonsterYaw, UserLoc, Range, FovAngle))
				{
					// C. 새로운 '가장 가까운 타겟' 갱신
					MinDistSq = DistSq;
					BestTargetId = UserIdx;
				}
			}
		}
	}

	return BestTargetId;
	
}

UINT32 ChatRoom::FindNearestMonsterInSight(const Location& UserLoc, float UserYaw, float Range, float FovAngle)
{
	UINT32 BestTargetId = 0xffffffff;
	float MinDistSq = Range * Range; // 탐색 범위의 제곱을 초기 최소 거리로 설정

	// 섹션 인덱스 계산
	int CenterX = GetSectionX(UserLoc.x);
	int CenterY = GetSectionY(UserLoc.y);

	// 주변 9개 섹션 순회
	for (int y = -1; y <= 1; ++y)
	{
		for (int x = -1; x <= 1; ++x)
		{
			int CheckX = CenterX + x;
			int CheckY = CenterY + y;

			if (CheckX < 0 || CheckX >= SectionCountX || CheckY < 0 || CheckY >= SectionCountY)
				continue;

			int SectionIdx = (CheckY * SectionCountX) + CheckX;


			// 몬스터 순회
			for (UINT32 MonsterIdx : GameSectionsOnlyMonster[SectionIdx])
			{
				//맵에 있는지 검사하는 로직이 필요 할 수 있음
				Location MonsterLoc = ObjectIdToMonsterPtr[MonsterIdx]->ActorPoint;

				float dx = MonsterLoc.x - UserLoc.x;
				float dy = MonsterLoc.y - UserLoc.y;
				float DistSq = (dx * dx) + (dy * dy);

				if (DistSq >= MinDistSq) continue;

				// 시야각 체크

				if (IsInSight(UserLoc, UserYaw, MonsterLoc, Range, FovAngle))
				{
					MinDistSq = DistSq;
					BestTargetId = MonsterIdx;
				}
			}
		}
	}
	//if(Damage!=0.f) ObjectIdToMonsterPtr[BestTargetId]->OnDamaged(Damage);//가장 가까운 적만 피격판정
	return BestTargetId;
}

bool ChatRoom::IsInSight(const Location& MonsterLoc, float MonsterYaw, const Location& TargetLoc, float Range, float FovAngle)
{
	// 1. 벡터 계산 (타겟 - 몬스터)
	float dx = TargetLoc.x - MonsterLoc.x;
	float dy = TargetLoc.y - MonsterLoc.y;

	// 2. 거리 체크 (Squared Distance) - sqrt 없이 1차 필터링
	float DistSq = (dx * dx) + (dy * dy);
	if (DistSq > Range * Range)
	{
		return false; // 사거리 밖임
	}

	// 3. 실제 거리 계산 (단위 벡터를 만들기 위함)
	float Dist = std::sqrt(DistSq);

	// 거리가 너무 가까우면(거의 겹침) 무조건 보인다고 판정 (Divide by Zero 방지)
	if (Dist < 0.001f)
	{
		return true;
	}

	// 4. 타겟 방향 벡터 정규화 (Normalize)
	float DirX = dx / Dist;
	float DirY = dy / Dist;

	// 5. 몬스터의 시선 벡터(Forward Vector) 계산
	// [중요] 언리얼 좌표계 기준: Yaw 0도 = X축(+1, 0) 방향
	// 만약 클라가 Unity라면 (sin, cos) 순서가 될 수 있음. 언리얼은 (cos, sin)
	float LookRad = MonsterYaw * TO_RAD;
	float LookX = std::cos(LookRad);
	float LookY = std::sin(LookRad);

	// 6. 내적(Dot Product) 계산
	// 두 유닛 벡터의 내적 = cos(사이각)
	float DotValue = (LookX * DirX) + (LookY * DirY);

	// 7. 시야각 임계값 계산
	// FovAngle이 90도라면, 중심축 기준으로 좌우 45도까지 허용
	// cos(45도)보다 내적값이 크면 시야 안임
	float HalfAngle = FovAngle / 2.0f;
	float Threshold = std::cos(HalfAngle * TO_RAD);

	// 내적값이 임계값보다 크면 각도가 더 좁다는 뜻 (즉, 시야 안)
	return DotValue >= Threshold;
}

Location ChatRoom::GetUserLocation(UINT32 Userid)
{
	return roomManager->ClientSessionPtr->GetClient(Userid)->GetLocate();
}

void ChatRoom::OnUserDamage(UINT32 AttackObjectId, UINT32 DamagedUsertId, float Damage, UINT32 Section)
{
	NoticeDamage Packet;
	Packet.CurrentHealth= roomManager->ClientSessionPtr->OnDamage(DamagedUsertId, Damage);
	//printf("%d번 유저 공격당함 남은 체력 : %f\n", DamagedUsertId, Packet.CurrentHealth);
	Packet.Damage = Damage;
	Packet.ObjectId = DamagedUsertId;
	Packet.PacketId = PACKET_ID::ON_DAMAGED_USER;
	Packet.PacketSize = sizeof(Packet);
	LpPacket SendPacket;
	SendPacket.PacketSize = Packet.PacketSize;
	SendPacket.pData = (char*)&Packet;
	BroadCastAllRoomUser(roomManager->MessageSender, SendPacket);
}
