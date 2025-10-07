#pragma once
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "mswsock.lib")
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <mswsock.h>

#define MAX_SOCKBUF 1024//최대 버퍼 크기

//overlappedio의 타입
enum class IOOperation
{
	RECV,
	SEND,
	ACCEPT
};

//overlapped 구조체 확장
struct OverlappedEx
{
	WSAOVERLAPPED m_wsaOverlapped;//overlap io 구조체 무조건 첫번째 인자로 넣어야함
	SOCKET SocketClient;//클라이언트 소켓
	WSABUF wasBuf;
	IOOperation Operation;//작업 종류
	UINT32 ClientIdx;//해당 작업을 요청한 클라이언트 Id
};

//클라이언트 구조체
struct ClientInfo
{
	SOCKET SocketClient;
	OverlappedEx RecvOverlappedEx;//수신용 overlap io 변수
	OverlappedEx SendOverlappedEx;//발신용 overlap io 변수
	OverlappedEx AcceptOverlappedEx;//접속용 overlap io 변수
	SOCKADDR_IN ClientAddr;


	int nAddrLen;
	bool Isconnected;//연결 상태
	char RecvBuf[MAX_SOCKBUF];
	char SendBuf[MAX_SOCKBUF];
	char AcceptBuf[MAX_SOCKBUF];

	UINT32 idx;//해당 클라이언트의 Id
	ClientInfo()
	{
		ZeroMemory(&RecvOverlappedEx, sizeof(OverlappedEx));
		ZeroMemory(&SendOverlappedEx, sizeof(OverlappedEx));
		ZeroMemory(&AcceptOverlappedEx, sizeof(OverlappedEx));
		SocketClient = INVALID_SOCKET;
	}
	bool IsValid()
	{
		if (SocketClient == INVALID_SOCKET) return false;
		return true;
	}
};