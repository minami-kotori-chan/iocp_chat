#include "Iocp.h"

//소켓 초기화 함수
bool IocpServer::InitSocket()
{
	WSADATA wsaData;
	int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);//winsock초기화
	if (nRet != 0) {
		printf("[에러] WSAStartup()함수 실패 : %d\n", WSAGetLastError());
		return false;
	}
	//tcp형 overlapped io 소켓 생성
	mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

	if (mListenSocket == INVALID_SOCKET) {
		printf("[에러] socket()함수 실패 : %d\n", WSAGetLastError());
		return false;
	}
	printf("소켓 초기화 성공\n");
	return true;
}

//서버 주소 등록 + 소켓 바인딩
bool IocpServer::BindandListen(int nBindPort)
{
	SOCKADDR_IN stServerAddr;//sockaddr은 단순히 sin_familly와 주소만을 저장해서 주소와포트가 조합되어 읽고쓰기가 불편함 그래서 sockaddr_in을 써서 ipv4의 경우 sockaddr_in을 써서 좀더 편하게 읽고쓰기를 한다.(sockaddr_in은 sin_familly가 항상 af_inet이 되어야한다)
	stServerAddr.sin_family = AF_INET;
	stServerAddr.sin_port = htons(nBindPort);//리틀엔디안때문에 포트번호를 바로쓰지않고 htons함수를 사용함(포트설정)
	stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);//모든 접속을 허용(만약 한 아이피만 받고싶으면 inet_addr함수 이용)

	//위 서버정보와 LISTEN소켓 바인딩
	int nRet = bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
	if (nRet != 0) {
		printf("[에러] bind()함수 실패 : %d\n", WSAGetLastError());
		return false;
	}

	nRet = listen(mListenSocket, 5);//2번째 인자는 백로그 큐의 개수를 지정함 백로그는 tcpip연결이 완료된(3way handshake가 완료된)클라이언트들이 accpet되기전에 대기하는 큐라고 생각하면 됨
	if (nRet != 0) {
		printf("[에러] listen()함수 실패 : %d\n", WSAGetLastError());
		return false;
	}

	printf("서버 등록 완료");
	return true;
}

//서버 실행 함수
bool IocpServer::StartServer(const UINT32 maxClientCount)
{

	CreateClient(maxClientCount);
	mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKERTHREAD);//IOCP객체 생성

	CreateIoCompletionPort((HANDLE)mListenSocket, mIOCPHandle, 0, 0);//리슨소켓을 iocp에 등록
	if (mIOCPHandle == NULL) {
		printf("[에러] CreateIoCompletionPort()함수 실패: %d\n", GetLastError());
		return false;
	}

	CreateWorkThread();//워커스레드 생성
	CreateAccepterThread();//접속용 스레드 생성


	Start(maxClientCount);
	printf("\n서버실행완료\n");
	return true;
}

//iocp객체와 completionkey연결
bool IocpServer::BindIOCompletionPort(ClientInfo* pClientInfo)
{
	HANDLE hIOCP = CreateIoCompletionPort((HANDLE)pClientInfo->SocketClient, mIOCPHandle, (ULONG_PTR)(pClientInfo), 0);//소켓을 iocp에 바인딩
	if (hIOCP == NULL || hIOCP != mIOCPHandle) {
		printf("[에러] CreateIoCompletionPort()함수 실패: %d\n", GetLastError());
		return false;
	}
	return true;
}

void IocpServer::CreateClient(const UINT32 maxClientCount)
{
	for (UINT32 i = 0; i < maxClientCount; i++) {
		mClientInfos.emplace_back(new ClientInfo());
		mClientInfos[i]->idx = i;
	}
}

void IocpServer::CreateWorkThread()
{
	//스레드 풀에 대기상태로 넣을 스레드 수는 (cpu개수*2)+1이 권장된다함 검증필요
	for (int i = 0; i < MAX_WORKERTHREAD; i++) {
		mIOWorkerThread.emplace_back([this]() {WorkerThread(); });
	}
	printf("WokerThread 시작..\n");
}

void IocpServer::CreateAccepterThread()
{
	mAccepterThread = std::thread([this]() {AccepterThread(); });
	printf("AccepterThread 시작..\n");
}

void IocpServer::WorkerThread()
{
	//completionkye받을 변수
	ClientInfo* pClientInfo = NULL;
	//함수 호출 성공 여부
	BOOL bSuccess = TRUE;
	//overlapped io에서 전송된 데이터 크기
	DWORD dwIoSize = 0;
	//overlapped구조체 받을 포인터
	LPOVERLAPPED lpOverlapped = NULL;

	while (mIsWorkerRun)
	{
		//////////////////////////////////////////////////////
		//이 함수로 인해 쓰레드들은 WaitingThread Queue에
		//대기 상태로 들어가게 된다.
		//완료된 Overlapped I/O작업이 발생하면 IOCP Queue에서
		//완료된 작업을 가져와 뒤 처리를 한다.
		//그리고 PostQueuedCompletionStatus()함수에의해 사용자
		//메세지가 도착되면 쓰레드를 종료한다.
		//////////////////////////////////////////////////////
		bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
			&dwIoSize, //실제 전송된 바이트
			(PULONG_PTR)&pClientInfo, // completionkey
			&lpOverlapped, //overlapped io 객체
			INFINITE//대기시간
		);

		//스레드 종료 메세지 수신
		if (bSuccess == TRUE && dwIoSize == 0 && lpOverlapped == NULL) {
			mIsWorkerRun = false;
			continue;
		}
		if (lpOverlapped == NULL) {//오버랩 구조체가 비어있는 경우 무시하고 다음으로
			continue;
		}
		OverlappedEx* pOverlappedEx = (OverlappedEx*)lpOverlapped;
		if (bSuccess == FALSE/*rst패킷수신*/ || (dwIoSize == 0 && bSuccess == TRUE && pOverlappedEx != NULL && pOverlappedEx->Operation != IOOperation::ACCEPT)) {//client의 접속 끊음(fin패킷수신)
			//printf("socket(%d) 접속 끊김\n", (int)pClientInfo->m_socketClient);
			CloseSocket(pClientInfo);
			continue;
		}

		//////////////////////////////////////////////////////
		//실제 IO처리 시작 
		//지금은 if else로 하고 있는데 나중에 종류가 많아지면 함수포인터로 관리해야 편할듯함
		//////////////////////////////////////////////////////
		

		//recv 결과 처리
		if (pOverlappedEx->Operation == IOOperation::RECV) {
			pClientInfo->RecvBuf[dwIoSize] = 0;//소켓에서 버퍼에 쓰는 결과에는 널문자가 들어가지 않아서 직접 써줘야함(문자열인 경우)
			printf("수신한 문자열 : %s ", &(pClientInfo->RecvBuf[5]));//패킷헤더 구조상 5번째부터 문자열이기 때문에 5번째부터 출력(테스트용 코드임)
			OnRecv(pClientInfo->idx, pClientInfo->RecvBuf, dwIoSize);//가상함수호출

			//비동기 수신 처리
			BindRecv(pClientInfo);//근데 이렇게 하면 이 클라이언트인포에서 동시에 여러개의 수신요청이 날아오면 문제가 발생하지 않나?

		}
		//send 결과 처리
		else if (pOverlappedEx->Operation == IOOperation::SEND) {
			pClientInfo->OnSendComplete();
			OnSendComplete(pClientInfo->idx);
		}
		//accept 결과 처리
		else if (pOverlappedEx->Operation == IOOperation::ACCEPT)
		{
			//이경우 pClientInfo가 null이기 때문에(아직 iocp핸들에 연결을 안했으니까 당연히 nullptr임)pOverlappedEx를 이용해서 클라이언트구조체에 접근해야함
			ClientCnt++;//접속인원수 증가시키기 락프리라서 이대로 상관없음
			BindIOCompletionPort(GetClientInfo(pOverlappedEx->ClientIdx));//iocp에 소켓 바인딩
			BindRecv(GetClientInfo(pOverlappedEx->ClientIdx));//비동기 수신처리
			OnConnect(pOverlappedEx->ClientIdx);//가상함수실행
		}
		//예외 발생
		else {
			printf("socket(%d)에서 예외상황\n", (int)pClientInfo->SocketClient);
		}


	}
}

//accept스레드
void IocpServer::AccepterThread2()
{
	while (mIsAcceptRun)
	{
		for (const auto& clientInfo : mClientInfos) {
			if (clientInfo->IsValid())
			{
				continue;
			}
			PostAccept(clientInfo);

		}
		//printf("\n생성종료\n");
		std::this_thread::sleep_for(std::chrono::milliseconds(32));//슬립, 이것도 나중에 생성자 소비자로 만들어도 좋을듯 함
	}
}

void IocpServer::AccepterThread()
{
	for (const auto& clientInfo : mClientInfos) {
		if (clientInfo->IsValid()) continue;
		PostAccept(clientInfo);
	}
	while (mIsAcceptRun)
	{
		UINT32 idx=0;
		bool QueCondition = false;
		{
			std::unique_lock<std::mutex> lock(EmptySocketQueLock);
			RecvPacketCV.wait(lock, [this] { return !EmptySocketQue.empty() || !mIsAcceptRun; });
			if (mIsAcceptRun == false) break; 
			
			if (!EmptySocketQue.empty()) {
				UINT32 idx = EmptySocketQue.front();
				EmptySocketQue.pop_front();
				QueCondition = true;
			}
		}
		if(QueCondition) PostAccept(GetClientInfo(idx));
		
	}
}

//accept처리 함수
bool IocpServer::PostAccept(ClientInfo* pClientInfo)
{
	DWORD dwRecvNumBytes = 0;

	//wasbuf에 버퍼 주소 지정
	DWORD bytes;
	pClientInfo->SocketClient = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == pClientInfo->SocketClient)
	{
		printf_s("client Socket WSASocket Error : %d\n", GetLastError());
		return false;
	}
	pClientInfo->AcceptOverlappedEx.Operation = IOOperation::ACCEPT;
	pClientInfo->AcceptOverlappedEx.ClientIdx = pClientInfo->idx;//이 id를 통해서 추후 pClientInfo에 접근
	if (mListenSocket == INVALID_SOCKET)
	{
		printf("ERROR");
	}

	bool nRet = AcceptEx(mListenSocket,
		pClientInfo->SocketClient,
		pClientInfo->AcceptBuf,
		0,
		sizeof(SOCKADDR_IN) + 16,
		sizeof(SOCKADDR_IN) + 16,
		&bytes,
		(LPWSAOVERLAPPED) & (pClientInfo->AcceptOverlappedEx)
	);
	// 오류 처리 로직
	if (nRet == FALSE && WSAGetLastError() != WSA_IO_PENDING)
	{
		printf("AcceptEx 즉시 실패! 오류 코드: %d\n", WSAGetLastError());
		return false;
	}
	return true;
}

//비동기 RECV 바인딩 함수
bool IocpServer::BindRecv(ClientInfo* pClientInfo)
{
	DWORD dwFlag = 0;
	DWORD dwRecvNumBytes = 0;

	//WSABUF에 데이터 입력(비동기 io를 위해서)
	pClientInfo->RecvOverlappedEx.wasBuf.len = MAX_SOCKBUF;
	pClientInfo->RecvOverlappedEx.wasBuf.buf = pClientInfo->RecvBuf;
	pClientInfo->RecvOverlappedEx.Operation = IOOperation::RECV;

	//비동기 recv함수 인자는 각각 (소켓,wsabuf,wsabuf의 배열 개수, 데이터 크기,행동방식,overlapped 구조체, 콜백 이벤트)
	//추가로 비동기 방식에서는 4번째 인자는 리턴값이 없음
	//행동방식의 경우 일반적인 tcp통신에서는 필요없다고 함


	int nRet = WSARecv(pClientInfo->SocketClient,
		&(pClientInfo->RecvOverlappedEx.wasBuf),
		1,
		&dwRecvNumBytes,
		&dwFlag,
		(LPWSAOVERLAPPED) & (pClientInfo->RecvOverlappedEx), //OVERLAPPED 구조체 넣기
		NULL
	);

	if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING)) {
		printf("[에러] WSARecv()함수 실패 : %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

//비어있는 클라이언트 포인터 반환 함수 비동기 accept이기 때문에 필요는 없음
ClientInfo* IocpServer::GetEmptyClientInfo()
{
	for (auto& client : mClientInfos) {
		if (INVALID_SOCKET == client->SocketClient) {
			return client;
		}
	}
	return nullptr;
}

void IocpServer::SendData(UINT32 idx, char* pData,int pSize)
{
	/*packet으로 포장하는건 애플리케이션에 위임하기로 함 따라서 해당 함수는 그냥 클라이언트 구조체의 발송함수를 콜하는 역할만 함
	MessagePacket p;
	p.PacketId = PACKET_ID::MESSAGE;
	p.PacketSize = sizeof(PacketHead)+ pSize;
	CopyMemory(p.Msg, pData, pSize);
	p.DataSize = pSize;

	*/
	GetClientInfo(idx)->SetSendData(idx,pData, pSize);


}

void IocpServer::CloseSocket(ClientInfo* pClientInfo, bool bIsForce)
{
	pClientInfo->CloseSocket(bIsForce);
	
	ClientCnt--;
	OnDisConnect(pClientInfo->idx);
}

void IocpServer::DestroyThread()
{
	mIsWorkerRun = false;
	CloseHandle(mIOCPHandle);

	for (auto& th : mIOWorkerThread)
	{
		if (th.joinable())
		{
			th.join();
		}
	}

	//Accepter 쓰레드를 종요한다.
	mIsAcceptRun = false;
	RecvPacketCV.notify_all();//모든 accept스레드를 깨움

	if (mAccepterThread.joinable())
	{
		mAccepterThread.join();
	}
	EmptySocketQue.clear();//accept 큐 비움

	closesocket(mListenSocket);
}

void IocpServer::PushEmptyClient(UINT32 Idx)
{
	std::lock_guard<std::mutex> lock(EmptySocketQueLock);
	EmptySocketQue.push_back(Idx);
	RecvPacketCV.notify_one();
}