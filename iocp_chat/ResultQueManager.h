#pragma once
#include <deque>
#include <mutex>
#include <optional>
#include "Packet.h"

class ResultQueManager
{
public:

	void PushResultQue(LPacket& ResultPacket)
	{
		{
		std::lock_guard<std::mutex> lock(ResultQueLock);
		ResultQue.push_back(ResultPacket);
		}
		ResultQueCV.notify_one();
	}
	std::optional<LPacket> PopResultQue()
	{
		std::unique_lock<std::mutex> lock(ResultQueLock);
		ResultQueCV.wait(lock, [this] {return !ResultQue.empty() || ResultQueStop; });

		if (ResultQueStop == true && ResultQue.empty()) return std::nullopt;//종료 조건 확인


		LPacket ResultPacket = ResultQue.front();
		ResultQue.pop_front();
		return ResultPacket;
	}

	void ResultQueManagerStop()
	{
		ResultQueStop = true;
	}

private:

	std::mutex ResultQueLock;
	std::condition_variable ResultQueCV;
	std::deque<LPacket> ResultQue;
	bool ResultQueStop = false;
};