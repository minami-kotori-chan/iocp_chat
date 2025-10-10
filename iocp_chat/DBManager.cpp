#include "DBManager.h"

void DBManager::Init(const char* host, const char* user, const char* passwd, unsigned int port)
{
	if (conn == nullptr)
	{
		mysql_library_init(0, NULL, NULL);//멀티스레드 환경이라면 써줘야함

		conn = mysql_init(NULL);

		if (mysql_real_connect(conn, host, user, passwd, db, port, NULL, 0) == NULL) {
			// 연결 실패 시 에러 메시지 출력
			printf("연결 실패");
		}
		printf("서버 버전 정보 : %s", mysql_get_server_info(conn));
	}
}

bool DBManager::LoginRequest(const char* username, const char* userpw)
{
	// 1. 미리 준비된 LOGIN_REQ Statement 핸들을 가져옵니다.
	auto it = preparedStatements.find(DB_TYPE::LOGIN_REQ);
	if (it == preparedStatements.end())
	{
		fprintf(stderr, "Error: LOGIN_REQ statement not prepared.\n");
		return false;
	}
	MYSQL_STMT* stmt = it->second;

	// 2. 입력 파라미터 바인딩: SQL의 '?' 부분에 username을 채워 넣습니다.
	MYSQL_BIND param_bind[1];
	memset(param_bind, 0, sizeof(param_bind)); // MYSQL_BIND 배열은 항상 0으로 초기화

	unsigned long username_len = strlen(username);

	param_bind[0].buffer_type = MYSQL_TYPE_STRING;
	param_bind[0].buffer = (char*)username;
	param_bind[0].buffer_length = username_len;

	if (mysql_stmt_bind_param(stmt, param_bind) != 0)
	{
		fprintf(stderr, "Error mysql_stmt_bind_param: %s\n", mysql_stmt_error(stmt));
		return false;
	}

	// 3. 쿼리를 실행합니다.
	if (mysql_stmt_execute(stmt) != 0)
	{
		fprintf(stderr, "Error mysql_stmt_execute: %s\n", mysql_stmt_error(stmt));
		return false;
	}

	// 4. 결과값 바인딩: DB에서 가져올 password_hash를 담을 변수를 지정합니다.
	MYSQL_BIND result_bind[1];
	memset(result_bind, 0, sizeof(result_bind));

	char fetched_password_hash[256]; // DB 결과를 저장할 버퍼
	//my_ulonglong hash_length = 0;    
	unsigned long hash_length;// 실제 데이터 길이를 저장할 변수

	result_bind[0].buffer_type = MYSQL_TYPE_STRING;
	result_bind[0].buffer = fetched_password_hash;
	result_bind[0].buffer_length = sizeof(fetched_password_hash);
	result_bind[0].length = &hash_length;

	//최적화 가능성이 보이는 부분

	if (mysql_stmt_bind_result(stmt, result_bind) != 0)
	{
		fprintf(stderr, "Error mysql_stmt_bind_result: %s\n", mysql_stmt_error(stmt));
		return false;
	}

	// 5. 결과셋을 클라이언트로 가져옵니다. (결과가 있는지 확인하기 위해)
	if (mysql_stmt_store_result(stmt) != 0)
	{
		fprintf(stderr, "Error mysql_stmt_store_result: %s\n", mysql_stmt_error(stmt));
		return false;
	}

	bool login_success = false;

	// 6. 결과가 정확히 1개 행일 때만 로그인 절차를 진행합니다.
	if (mysql_stmt_num_rows(stmt) == 1)
	{
		// 7. 행 데이터를 바인딩된 변수(fetched_password_hash)로 가져옵니다.
		if (mysql_stmt_fetch(stmt) == 0) // 성공 시 0 반환
		{
			// 8. 입력된 비밀번호와 DB에서 가져온 비밀번호를 비교합니다.
			if (strcmp(userpw, fetched_password_hash) == 0)
			{
				login_success = true; // 일치하면 로그인 성공
			}
		}
	}

	// 9. 다음 쿼리를 위해 statement 상태를 초기화합니다.
	mysql_stmt_free_result(stmt); // 결과셋 메모리 해제
	mysql_stmt_reset(stmt);       // 파라미터와 결과 바인딩 초기화

	return login_success;
}

bool DBManager::SignUpRequest(const char* username, const char* userpw)
{
	// 미리 준비된 SIGNUP_REQ Statement 에서 MYSQL_STMT 가져옴
	auto it = preparedStatements.find(DB_TYPE::SIGNUP_REQ);
	if (it == preparedStatements.end())
	{
		fprintf(stderr, "Error: SIGNUP_REQ statement not prepared.\n");
		return false;
	}
	MYSQL_STMT* stmt = it->second;

	// 입력 파라미터 바인딩: SQL의 ? 두 개에 username과 userpw를 순서대로 채워 넣음
	MYSQL_BIND params[2];
	memset(params, 0, sizeof(params));

	unsigned long username_len = strlen(username);
	unsigned long userpw_len = strlen(userpw);

	// 첫 번째 username
	params[0].buffer_type = MYSQL_TYPE_STRING;
	params[0].buffer = (char*)username;
	params[0].buffer_length = username_len;

	// 두 번째 userpw
	params[1].buffer_type = MYSQL_TYPE_STRING;
	params[1].buffer = (char*)userpw;
	params[1].buffer_length = userpw_len;

	if (mysql_stmt_bind_param(stmt, params) != 0)
	{
		fprintf(stderr, "Error mysql_stmt_bind_param: %s\n", mysql_stmt_error(stmt));
		return false;
	}

	// 쿼리를 실행합니다.
	if (mysql_stmt_execute(stmt) != 0)
	{
		// username 컬럼에 UNIQUE 제약조건이 있다면, 아이디 중복 시 여기서 에러가 발생합니다.
		fprintf(stderr, "Error mysql_stmt_execute: %s\n", mysql_stmt_error(stmt));
		mysql_stmt_reset(stmt); // 실패하더라도 다음 사용을 위해 리셋
		return false;
	}

	// INSERT 결과 확인: 영향을 받은 행(row)의 개수를 확인
	// 성공적인 INSERT는 정확히 1개의 행에 영향을 줌
	bool SingupSuccess = (mysql_stmt_affected_rows(stmt) == 1);

	// 다음 요청을 위해 statement 상태를 초기화
	mysql_stmt_reset(stmt);

	return SingupSuccess;
}

bool DBManager::DeleteUserRequest(const char* username)
{
	// MYSQL_STMT얻기
	auto it = preparedStatements.find(DB_TYPE::DELETE_USER_REQ);
	if (it == preparedStatements.end())
	{
		fprintf(stderr, "Error: DELETE_USER_REQ statement not prepared.\n");
		return false;
	}
	MYSQL_STMT* stmt = it->second;

	// 입력 파라미터 바인딩
	MYSQL_BIND param[1];
	memset(param, 0, sizeof(param));

	unsigned long username_len = strlen(username);

	param[0].buffer_type = MYSQL_TYPE_STRING;
	param[0].buffer = (char*)username;
	param[0].buffer_length = username_len;

	if (mysql_stmt_bind_param(stmt, param) != 0)
	{
		fprintf(stderr, "Error mysql_stmt_bind_param: %s\n", mysql_stmt_error(stmt));
		return false;
	}

	// 쿼리를 실행
	if (mysql_stmt_execute(stmt) != 0)
	{
		fprintf(stderr, "Error mysql_stmt_execute: %s\n", mysql_stmt_error(stmt));
		mysql_stmt_reset(stmt); // 실패 시 리셋
		return false;
	}

	// DELETE 결과 확인: 영향을 받은 행(row)의 개수를 확인
	//만약 0이라면, 해당 username을 가진 사용자가 존재하지 않는다는 의미
	bool success = (mysql_stmt_affected_rows(stmt) == 1);

	// 5. 다음 요청을 위해 statement 상태를 초기화합니다.
	mysql_stmt_reset(stmt);

	return success;
}

bool DBManager::SetPrepareQuery(MYSQL* conn)
{
	// 1. 등록할 모든 쿼리를 정의합니다.
		// 굳이 해시로 만드는 이유는 enum으로된 db type관리가 용이해지므로
	const std::unordered_map<DB_TYPE, const char*> queryRegistry = {
		{ DB_TYPE::SIGNUP_REQ, "INSERT INTO users (username, password_hash) VALUES (?, ?)" },
		{ DB_TYPE::LOGIN_REQ,  "SELECT password_hash FROM users WHERE username = ?" },
		{ DB_TYPE::DELETE_USER_REQ,  "DELETE FROM users WHERE username = ?" },
	};

	printf("Preparing SQL statements...\n");

	// 2. 정의된 모든 쿼리를 순회하며 prepare 작업을 수행합니다.
	for (const auto& pair : queryRegistry)
	{
		DB_TYPE type = pair.first;
		const char* sql = pair.second;

		MYSQL_STMT* stmt = mysql_stmt_init(conn);
		if (stmt == nullptr)
		{
			printf("Error: mysql_stmt_init() failed.\n");
			return false;
		}

		// 3. 쿼리문을 DB가 이해하도록 컴파일(prepare)합니다.
		if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0)
		{
			printf("Error: mysql_stmt_prepare() failed for query type %hu - %s\n",
				static_cast<unsigned short>(type), mysql_stmt_error(stmt));

			mysql_stmt_close(stmt); // 실패 시 생성된 핸들은 닫아줍니다.
			return false;
		}

		// 4. 성공적으로 생성된 statement 핸들을 해시 테이블에 저장합니다.
		preparedStatements[type] = stmt;
		printf("  - Query type %hu prepared successfully.\n", static_cast<unsigned short>(type));
	}

	printf("All SQL statements prepared successfully.\n");
	return true;
}

