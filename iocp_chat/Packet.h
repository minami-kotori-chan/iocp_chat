#pragma once
#include <basetsd.h>

//각종 패킷들 선언
#define MAX_PACKET_SIZE 1024

//해당 파일의 모든 구조체들은 직렬화를 해야하기때문에 오로지 데이터만 넣어놓을 것 절대 멤버함수를 만들지 말것
enum class PACKET_ID : UINT16
{
	INVALID = 0,

	ECHO_MESSAGE = 1,

	CONNECT_REQUEST = 100,
	CONNECT_RESPONSE = 101,


	LOGIN_REQUEST = 1000,
	LOGIN_RESPONSE = 1001,

	GUEST_REQUEST = 1010,
	GUEST_RESPONSE = 1011,

	LOGOUT_REQUEST = 1015,
	LOGOUT_RESPONSE = 1016,

	SIGNUP_REQUEST = 1020,
	SIGNUP_RESPONSE = 1021,

	DELETE_USER_REQUEST = 1030,
	DELETE_USER_RESPONSE = 1031,

	MESSAGE_REQUEST = 2000,
	MESSAGE_RESPONSE = 2001,

	GAME_MESSAGE_REQUEST = 2002,
	GAME_MESSAGE_RESPONSE = 2003,

	ENTER_ROOM_REQUEST = 2010,
	ENTER_ROOM_RESPONSE = 2011,

	EXIT_ROOM_REQUEST = 2012,
	EXIT_ROOM_RESPONSE = 2013,

	GAME_ROOM_ENTER_REQUEST = 2014,
	GAME_ROOM_ENTER_RESPONSE = 2015,

	GAME_ROOM_EXIT_REQUEST = 2016,
	GAME_ROOM_EXIT_RESPONSE = 2017,

	NOTICE_ROOM_NEW_USER = 3000,
	NOTICE_ROOM_EXIT_USER = 3001,

	UNIT_MOVING_DATA_REQUEST = 4000,//플레이어 좌표 데이터
	UNIT_MOVING_DATA_RESPONSE = 4001,

	MONSTER_DATA = 4002,
	MONSTER_ATTACK = 4010,

	USER_ATTACK_REQUEST = 4011,//유저의 공격요정

	ON_DAMAGED_USER = 5000,//유저가 데미지를 받은경우
	ON_DAMAGED_MONSTER = 5001,//몬스터가 데미지를 받은 경우
};

#pragma pack(push, 1)

struct PacketHead {
	UINT16 PacketSize;
	PACKET_ID PacketId;
};

struct LPacket {//패킷정보랑 실제 데이터의 주소를 저장하는 구조체 실제 데이터부분을 memcpy하는건 비효율적이므로 그냥 포인터로 받기 위해 새로 만든 구조체임
	PACKET_ID PacketId = PACKET_ID::INVALID;
	UINT16 PacketSize;
	UINT32 ClientIdx;
	//char* pData;
	char pData[MAX_PACKET_SIZE];
};
struct LpPacket {//LPacket포인터버전(큐에 넣고 뺄때 아니면 굳이 복사 안해도 되니까)
	PACKET_ID PacketId = PACKET_ID::INVALID;
	UINT16 PacketSize;
	UINT32 ClientIdx;
	char* pData;
	//char pData[MAX_PACKET_SIZE];
};
struct LPacketResult : LPacket {
	bool Success;
};

struct MessagePacket : PacketHead {
	char Msg[MAX_PACKET_SIZE - sizeof(PacketHead)];
	UINT16 DataSize;
};

struct MessagePacketResponse : PacketHead {
	UINT32 UserId;
	char Msg[MAX_PACKET_SIZE - sizeof(PacketHead)];
};

#define MAX_USERNAME_LENGTH 32
#define MAX_USERPASSWORD_LENGTH 32

struct LoginPacket : PacketHead {
	char UserName[MAX_USERNAME_LENGTH];
	char UserPW[MAX_USERPASSWORD_LENGTH];
	UINT16 NameSize;
	UINT16 PWSize;
};

struct GuestPacket : PacketHead {
	char UserName[MAX_USERNAME_LENGTH];
	UINT8 NameSize;
};

struct LoginResult : PacketHead {
	bool Success;
	UINT32 UserId;
	UINT8 NameSize;
	char UserName[MAX_USERNAME_LENGTH];
};

struct ResponsePacket : PacketHead {
	bool Success;
};

//방 관련 패킷

struct EnterRoomPacket : PacketHead {//방 접속 요청
	UINT32 RoomId;
};
struct EnterRoomPacketResponse : EnterRoomPacket {//방 접속 응답 (방번호+유저번호)
	UINT32 UserId;
};
struct ExitRoomPacket : PacketHead {//방 나가기 요청
	UINT32 UserId;
};
struct PacketMoveReq : PacketHead {//이동 동기화 패킷 (요청)
	float x;
	float y;
	float z;
	float Yaw; // 회전값
};
struct PacketMoveReqWithState : PacketMoveReq {//이동 동기화 패킷 (요청) (상태 추가)
	UINT8 State;
};

struct PacketMoveRes : PacketMoveReqWithState {//이동 동기화 패킷 (응답)
	UINT32 UserId;
	UINT32 Health;
};
struct MonsterMovingData : PacketMoveRes {//몬스터 이동동기화 및 스폰 패킷 (이동 동기화 패킷에서 타입만 추가됨)
	UINT32 MonsterType;
};
struct NoticeNewUserEnter : PacketHead {//들어온 유저(유저 아이디없음) 알림
	char UserName[MAX_USERNAME_LENGTH];
	UINT8 NameSize;
};
struct NoticeNewUserEnterGame : NoticeNewUserEnter {//들어온 유저 알림
	UINT32 UserId;
	PacketMoveReq PlayerLocate;
};

struct NoticeUserExit : PacketHead {//나간 유저 알림
	UINT32 UserId;
};
struct NoticeDamage : PacketHead {
	UINT32 ObjectId;
	float Damage;
	float CurrentHealth;
};
#pragma pack(pop)