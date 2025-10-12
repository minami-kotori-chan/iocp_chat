#pragma once
#include <unordered_map>
#include <vector>
#include <functional>
#include <atomic>

template <class ReturnType,class... Args>//첫 인자는 이 델리게이트에 바인딩 할 함수의 리턴타입, 다음은 함수의 매개변수 타입들
class DelegateManager 
{
public:
	uint32_t BindFunc(std::function<ReturnType(Args...)> InputFunc)
	{
		uint32_t uid = UniqueKey;
		UniqueKey++;
		DelegateMap[uid]= InputFunc;
		return uid;
	}
	void UnBindFunc(uint32_t uid)
	{
		DelegateMap.erase(uid);
	}
	void CallAllFunc(Args... args)
	{
		for (const auto& DMap : DelegateMap)
		{
			if (DMap.second)// 함수가 유효한지 확인
			{
				DMap.second(args...);
			}
		}
	}
	DelegateManager() = default;

private:


	std::unordered_map<uint32_t, std::function<ReturnType(Args...)>> DelegateMap;
	uint32_t UniqueKey = 0;
};