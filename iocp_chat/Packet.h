#pragma once

//각종 패킷들 선언
#define MAX_PACKET_SIZE 1024

enum class PACKET_ID: UINT16 
{
	INVALID=0,

	ECHO_MESSAGE=10,

	CONNECT_REQUEST=100,
	CONNECT_RESPONSE=101,


	LOGIN_REQUEST = 1000,
	LOGIN_RESPONSE = 1001,

	GUEST_REQUEST = 1010,
	GUEST_RESPONSE = 1011,

	MESSAGE=2000,
};

struct PacketHead {
	PACKET_ID PacketId;
	UINT16 PacketSize;
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

#define MAX_USERNAME_LENGTH 256
#define MAX_USERPASSWORD_LENGTH 256

struct LoginPacket : PacketHead {
	char UserName[MAX_USERNAME_LENGTH];
	char UserPW[MAX_USERPASSWORD_LENGTH];
};