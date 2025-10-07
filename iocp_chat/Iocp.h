#pragma once

#include "ClientInfos.h"
#include <vector>
#include <thread>
#include <atomic>

#define MAX_WORKERTHREAD 4  //쓰레드 풀에 넣을 쓰레드 수

class IocpServer
{
public:
	IocpServer(void) {}
	~IocpServer(void) { WSACleanup(); }

	//소켓 초기화 함수
	bool InitSocket();
	//서버 주소 등록 + 소켓 바인딩
	bool BindandListen(int nBindPort);

	//서버 실행시 실행될 함수
	bool StartServer(const UINT32 maxClientCount);

	ClientInfo* GetClientInfo(UINT32 idx)
	{
		return mClientInfos[idx];
	}
	void StopServer() { DestroyThread(); };
protected:
	//가상 함수 선언
	virtual void OnConnect(UINT32 idx) {};
	virtual void OnDisConnect(UINT32 idx) {};
	virtual void OnRecv(UINT32 idx) {};
	virtual void OnSendComplete(UINT32 idx) {};
	
	virtual void Start() {};//서버의 모든 초기화 완료이후 호출되는 함수

private:
	//iocp객체와 completionkey연결
	bool BindIOCompletionPort(ClientInfo* pClientInfo);


	//클라이언트 생성
	void CreateClient(const UINT32 maxClientCount);
	//워커 스레드 생성
	void CreateWorkThread();
	//accept 스레드 생성
	void CreateAccepterThread();
	//send 스레드 생성
	void CreateSendPacketThread();//send는 네트워크쪽이 아니라 애플리케이션단에서 전송하게해야함 따라서 이 함수는 추후 삭제해야함


	//워커스레드
	void WorkerThread();
	//비동기 접속처리를 위한 스레드
	void AccepterThread();
	//비동기 접속처리를 위한 함수
	bool PostAccept(ClientInfo* pClientInfo);

	//패킷수신 비동기처리를 위해 비동기함수에 바인딩
	bool BindRecv(ClientInfo* pClientInfo);


	//사용하지 않는 클라이언트 정보 구조체 반환
	ClientInfo* GetEmptyClientInfo();

	
	//패킷송신인데 이는 애플리케이션 코드에서 하게 할 예정이므로 작성만 하고 사용하지는 않을듯함
	bool SendMsg(ClientInfo* pClientInfo, char* pMsg, int nLen);
	//1-send를 위해서 클라이언트를 순회돌면서 버퍼가 차 있고 전송중이 아닐때는 send queue방식의 1-send를 구현하게 되면서 사용안하게 된 함수임
	void SendPacket();

	//소켓해제
	void CloseSocket(ClientInfo* pClientInfo, bool bIsForce = false);

	//스레드 제거
	void DestroyThread();

	//현재 클라이언트수 변경함수(함수없이 하면 하면 락부분 코드가 난잡해짐)
	void AddClientCnt();
	void SubClientCnt();

	//멤버 변수 영역//

	//스레드 종료를 위한 플래그 변수
	bool mIsWorkerRun = true;
	bool mIsAcceptRun = true;
	bool PacketProcessRun = true;

	//클라이언트 정보 배열
	std::vector<ClientInfo*> mClientInfos;
	//워커스레드 배열
	std::vector<std::thread> mIOWorkerThread;
	//accept 스레드
	std::thread mAccepterThread;
	//Send 스레드
	std::thread mSendPacket;

	//클라이언트의 접속을 받기위한 리슨 소켓
	SOCKET mListenSocket = INVALID_SOCKET;
	//iocp핸들
	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;

	//클라이언트 개수 락(처음에는 락을 쓰려했는데 생각해보니 락프리가 매우 효율적인 상황인것 같아서 락프리 쓰기로함
	//std::mutex ClientCntLock;

	//현재 접속중인 클라이언트 개수
	std::atomic<UINT32> ClientCnt{ 0 }; // 락프리로 연산하자
};