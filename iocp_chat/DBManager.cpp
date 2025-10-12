#include "DBManager.h"

void DBManager::Init(const char* host, const char* user, const char* passwd, unsigned int port, unsigned int ThreadCnt)
{
	Host = (char*)host;
	User = (char*)user;
	Passwd = (char*)passwd;
	Port = (int)port;

	CreateThread(ThreadCnt);
}

void DBManager::BindingFuncOnDelegate(DelegateManager<void, LPacket&>& pDM)
{
	//pDM.BindFunc(std::bind(&PushQueryRequest, this, std::placeholders::_1));//이렇게 바인딩 할 필요가 없이 람다로 하는게 훨씬 편하고 디버깅도 쉽다.
	pDM.BindFunc([this](LPacket& packet) {
		PushQueryRequest(packet);
		}
	);
	
}

void DBManager::BindFuncOnMap()
{
	DBRequestMap[DB_TYPE::LOGIN_REQUEST] = &DBManager::LoginReq;
	DBRequestMap[DB_TYPE::SIGNUP_REQUEST] = &DBManager::SignUpReq;
	DBRequestMap[DB_TYPE::DELETE_USER_REQUEST] = &DBManager::DeleteUserReq;
}

void DBManager::CreateThread(unsigned int ThreadCnt)
{
	for (unsigned int i = 0; i < ThreadCnt; i++){
		ConnectionThreads.emplace_back([this]() {ProcessQueryQue(); });
	}
	

}

void DBManager::CloseThread()
{
	RequestThreadRun = false;
	RequstQueCV.notify_all();
	for (auto& i : ConnectionThreads) {
		if (i.joinable()) {
			i.join();
		}
	}
}

void DBManager::PushQueryRequest(DB_Request& DRequest)
{
	std::lock_guard<std::mutex> lock(RequestQueLock);
	RequestQue.push_back(DRequest);
	RequstQueCV.notify_one();
}

DB_Request DBManager::PopQueryRequest()
{
	std::lock_guard<std::mutex> lock(RequestQueLock);
	DB_Request DRequest = RequestQue.front();
	RequestQue.pop_front();
	return DB_Request();
}


void DBManager::PushResultQue(DB_Result& DResult)
{
	std::lock_guard<std::mutex> lock(ResultQueLock);
	ResultQue.push_back(DResult);
	ResultQueCV.notify_one();
}

DB_Result DBManager::PopResultQue()
{
	std::lock_guard<std::mutex> lock(ResultQueLock);
	DB_Result DResult = ResultQue.front();
	ResultQue.pop_front();
	return DResult;
}

void DBManager::ProcessQueryQue()
{
	ConnectionManager* Connection = new ConnectionManager();
	Connection->Init(Host,User,Passwd,Port,db);

	while (RequestThreadRun)
	{
		DB_Request DbRequest;
		{
			std::unique_lock<std::mutex> lock(RequestQueLock);
			RequstQueCV.wait(lock, [this] {return !RequestQue.empty() || !RequestThreadRun; });

			if (RequestThreadRun == false) break;

			if (RequestQue.empty()) {
				continue;
			}
			DbRequest = PopQueryRequest();
		}

		if (DBRequestMap.find(DbRequest.PacketId) != DBRequestMap.end())
		{
			(DbRequest.pData)[DbRequest.PacketSize] = 0;//맨마지막에 널을 넣어줌
			(this->*(DBRequestMap[DbRequest.PacketId]))(*Connection, DbRequest);
		}

	}
	Connection->CloseConnection();

}

void DBManager::LoginReq(ConnectionManager& Connection, DB_Request& DRequest)
{
	LoginPacket* Lpacket = (LoginPacket*)(DRequest.pData);
	Connection.LoginRequest(Lpacket->UserName, Lpacket->UserPW);
}

void DBManager::SignUpReq(ConnectionManager& Connection, DB_Request& DRequest)
{
	LoginPacket* Lpacket = (LoginPacket*)(DRequest.pData);
	Connection.SignUpRequest(Lpacket->UserName, Lpacket->UserPW);
}

void DBManager::DeleteUserReq(ConnectionManager& Connection, DB_Request& DRequest)
{
	LoginPacket* Lpacket = (LoginPacket*)(DRequest.pData);
	Connection.DeleteUserRequest(Lpacket->UserName, Lpacket->UserPW);
}



