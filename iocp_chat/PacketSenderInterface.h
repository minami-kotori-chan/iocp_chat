#pragma once
#include <basetsd.h>

class PacketSenderInterface
{
public:
	virtual ~PacketSenderInterface() {}
	virtual void SendData(UINT32 idx,char *pData,int Psize) {}
};