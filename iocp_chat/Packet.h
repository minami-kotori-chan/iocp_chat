#pragma once

#define MAX_PACKET_SIZE 1024

enum class PACKET_ID: UINT16 
{
	INVALID=0,
	MESSAGE=1000,

};

struct PacketHead {
	PACKET_ID PacketId;
	UINT16 PacketSize;
};

struct MessagePacket : PacketHead {
	char Msg[MAX_PACKET_SIZE-sizeof(PacketHead)];
	UINT16 DataSize;
};

