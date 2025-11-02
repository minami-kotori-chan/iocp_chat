#pragma once
#include <basetsd.h>

//각종 패킷들 선언
#define MAX_PACKET_SIZE 1024

//해당 파일의 모든 구조체들은 직렬화를 해야하기때문에 오로지 데이터만 넣어놓을 것 절대 멤버함수를 만들지 말것
enum class PACKET_ID: UINT16 
{
	INVALID=0,

	ECHO_MESSAGE=1,

	CONNECT_REQUEST=100,
	CONNECT_RESPONSE=101,


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

	MESSAGE=2000,
};

struct PacketHead {
	UINT16 PacketSize;
	PACKET_ID PacketId;
};

struct LPacket {//패킷정보랑 실제 데이터의 주소를 저장하는 구조체 실제 데이터부분을 memcpy하는건 비효율적이므로 그냥 포인터로 받기 위해 새로 만든 구조체임
	PACKET_ID PacketId= PACKET_ID::INVALID;
	UINT16 PacketSize;
	UINT32 ClientIdx;
	char* pData;
};

struct MessagePacket : PacketHead {
	char Msg[MAX_PACKET_SIZE-sizeof(PacketHead)];
	UINT16 DataSize;
};

#define MAX_USERNAME_LENGTH 32
#define MAX_USERPASSWORD_LENGTH 32

struct LoginPacket : PacketHead {
	char UserName[MAX_USERNAME_LENGTH];
	char UserPW[MAX_USERPASSWORD_LENGTH];
};

struct ResponsePacket : PacketHead {
	bool Success;
};