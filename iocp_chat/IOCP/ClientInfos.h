#pragma once
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "mswsock.lib")
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <mswsock.h>
#include <mutex>


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
	char SendBuf[MAX_SOCKBUF * 8];
	char AcceptBuf[MAX_SOCKBUF];

	char SendingBuf[MAX_SOCKBUF * 8];//전송버퍼 헤드부터 테일까지 0번부터 복제해놓음

	int SendHead;//링버퍼 헤드(read)
	int SendTail;//링버퍼 테일(write)

	UINT32 idx;//해당 클라이언트의 Id

	bool IsSending = false;//전송중인지 확인용 변수

	std::mutex IsSendBufLock;//issending용 뮤텍스

	ClientInfo()
	{
		ZeroMemory(&RecvOverlappedEx, sizeof(OverlappedEx));
		ZeroMemory(&SendOverlappedEx, sizeof(OverlappedEx));
		ZeroMemory(&AcceptOverlappedEx, sizeof(OverlappedEx));
		SocketClient = INVALID_SOCKET;

		// Head와 Tail 초기화
		SendHead = 0;
		SendTail = 0;
	}
	bool IsValid()
	{
		if (SocketClient == INVALID_SOCKET) return false;
		return true;
	}

	void SetSendData(UINT32 ClientId,char *pData,UINT16 DataSize)
	{
		std::lock_guard<std::mutex> Lock(IsSendBufLock);
		if (SendTail + DataSize > sizeof(SendBuf)) {//공간이 모자라면 쌓여있는 버퍼를 처음으로 옮겨서 씀 만약 그래도 모자라다면 버퍼오버플로우가 나므로 클라이언트를 연결해제시킬것(정상적인 경우 버퍼가 모자랄수없음)
			if (SendTail - SendHead == 0) {
				SendHead = 0;
				SendTail = 0;
			}
			else {
				CopyMemory(&SendBuf[0], &SendBuf[SendHead], SendTail - SendHead);
				SendTail = SendTail - SendHead;
				SendHead = 0;
			}
			
		}
		CopyMemory(&SendBuf[SendTail], pData, DataSize);
		SendTail += DataSize;

		SendDataOnBuf();
	}

	void SendDataOnBuf()//락 상태에서만 호출할 것
	{
		if (IsSending == true) return;
		if (SendHead == SendTail) return;
		
		IsSending = true;

		CopyMemory(&SendingBuf[0], &SendBuf[SendHead], SendTail - SendHead);//데이터를 처음부터 다시 적음 << 속도에 문제가 생기면 최우선적으로 여기부터 수정할것 메모리 복사하느라 오래걸릴듯함
		SendTail = SendTail - SendHead;
		SendHead = 0;
		
		SendOverlappedEx.wasBuf.len = SendTail - SendHead;
		SendOverlappedEx.wasBuf.buf = SendingBuf;
		SendOverlappedEx.Operation = IOOperation::SEND;
		SendOverlappedEx.SocketClient = SocketClient;
		SendOverlappedEx.ClientIdx = idx;

		DWORD dwRecvNumBytes = 0;
		int nRet = WSASend(SocketClient,
			&(SendOverlappedEx.wasBuf),
			1,
			&dwRecvNumBytes,
			0,
			(LPWSAOVERLAPPED) & (SendOverlappedEx),
			NULL);


		//socket_error이면 client socket이 끊어진걸로 처리한다.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[에러] WSASend()함수 실패 : %d\n", WSAGetLastError());
			return;
		}

		SendHead = SendTail;//버퍼 비우기

	}

	

	void OnSendComplete()
	{
		std::lock_guard<std::mutex> Lock(IsSendBufLock);
		IsSending = false;//lock필요
		printf("전송완료\n");
		SendDataOnBuf();
	}

	void CloseSocket(bool bIsForce)
	{
		linger stLinger = { 0,0 };

		//force가 true이면 so_linger, timeout 0으로 강제종료
		if (bIsForce == true)
		{
			stLinger.l_onoff = 1;
		}
		//소켓의 송수신 모두 중단시킴
		shutdown(SocketClient, SD_BOTH);

		//소켓 옵션 설정
		setsockopt(SocketClient, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		//소켓 연결 종료
		closesocket(SocketClient);//이를 통해 모든 소켓의 관련 정보가 정리됨 (바인딩 정보도 포함해서 정리됨)

		SocketClient = INVALID_SOCKET;
	}
};