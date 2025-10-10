#include "DBManager.h"

void DBManager::Init(const char* host, const char* user, const char* passwd, unsigned int port, unsigned int ThreadCnt)
{
	Host = (char*)host;
	User = (char*)user;
	Passwd = (char*)passwd;
	Port = (int)port;

	CreateThread(ThreadCnt);
}

void DBManager::BindFuncOnMap()
{
	DBRequestMap[DB_TYPE::LOGIN_REQ] = &DBManager::LoginReq;
	DBRequestMap[DB_TYPE::SIGNUP_REQ] = &DBManager::SignUpReq;
	DBRequestMap[DB_TYPE::DELETE_USER_REQ] = &DBManager::DeleteUserReq;
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

		if (DBRequestMap.find(DbRequest.Dtype) != DBRequestMap.end())
		{
			(this->*(DBRequestMap[DbRequest.Dtype]))(*Connection, DbRequest);
		}

	}
	Connection->CloseConnection();

}

void DBManager::LoginReq(ConnectionManager& Connection, DB_Request& DRequest)
{
	Connection.LoginRequest(DRequest.params.login.username, DRequest.params.login.password);
}

void DBManager::SignUpReq(ConnectionManager& Connection, DB_Request& DRequest)
{
	Connection.SignUpRequest(DRequest.params.signup.username, DRequest.params.signup.password);
}

void DBManager::DeleteUserReq(ConnectionManager& Connection, DB_Request& DRequest)
{
	Connection.DeleteUserRequest(DRequest.params.removeUser.username, DRequest.params.removeUser.password);
}



