#pragma once
#include <deque>
#include <mutex>
#include <optional>
#include "Packet.h"

class ResultQueManager
{
public:

	void PushResultQue(LPacketResult& ResultPacket)
	{
		{
		std::lock_guard<std::mutex> lock(ResultQueLock);
		if (ResultQue.size() >= MaxQueueSize) return;
		ResultQue.push_back(ResultPacket);
		}
		ResultQueCV.notify_one();
	}
	std::optional<LPacketResult> PopResultQue()
	{
		std::unique_lock<std::mutex> lock(ResultQueLock);
		ResultQueCV.wait(lock, [this] {return !ResultQue.empty() || ResultQueStop; });

		if (ResultQueStop == true && ResultQue.empty()) return std::nullopt;//종료 조건 확인


		LPacketResult ResultPacket = ResultQue.front();
		ResultQue.pop_front();
		return ResultPacket;
	}

	void ResultQueManagerStop()
	{
		ResultQueStop = true;
		ResultQueCV.notify_all();
	}

private:
	const UINT32 MaxQueueSize = 1024 * 1024 * 128;//최대 3GB
	std::mutex ResultQueLock;
	std::condition_variable ResultQueCV;
	std::deque<LPacketResult> ResultQue;
	bool ResultQueStop = false;
};