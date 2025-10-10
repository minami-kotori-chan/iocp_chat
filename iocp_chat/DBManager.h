#pragma once
#pragma comment(lib, "libmariadb.lib")
#include <mysql.h>
#include <string>
#include <vector>
#include <unordered_map>

enum class DB_TYPE : unsigned short
{
	SIGNUP_REQ=10,
	LOGIN_REQ=11,



	DELETE_USER_REQ = 1000,
};

class DBManager
{
public:
	DBManager(){}
	DBManager(const char* host, const char* user, const char* passwd, unsigned int port)
	{
		Init(host, user, passwd, port);
	}
	~DBManager()
	{
		ClosePrepareQuery();//prepared statement 제거
		mysql_close(conn); // 핸들러 자원 해제
	}

	//DB초기화 작업 conn할당 추후 스레드 풀 기능도 여기서 만들어야함
	void Init(const char* host, const char* user, const char* passwd, unsigned int port);
	
	//로그인 요청 처리
	bool LoginRequest(const char* username, const char* userpw);
	//회원가입 요청 처리
	bool SignUpRequest(const char* username, const char* userpw);
	//삭제 요청 처리
	bool DeleteUserRequest(const char* username);

private:
	bool SetPrepareQuery(MYSQL* conn);


	void ClosePrepareQuery()
	{
		for (auto& i : preparedStatements) {
			if (i.second != nullptr) {
				mysql_stmt_close(i.second);
			}
		}
	}


	MYSQL* conn = NULL;
	const char* db = "mysql";// 접속할 데이터베이스 이름
	std::unordered_map<DB_TYPE, MYSQL_STMT*> preparedStatements;
};