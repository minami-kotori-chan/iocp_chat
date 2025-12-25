#pragma once
#include <basetsd.h>
#include <random>

struct CharacterData
{
	float x=0;
	float y=0;
	float z=0;
	float Yaw=0;

	UINT32 health=100;


    void SetRandomForTest()
    {
        // static을 사용하여 함수가 여러 번 호출되어도 엔진 초기화는 한 번만 수행
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> dist(1.0f, 100.0f);

        x = dist(gen);
        y = dist(gen);
        z = dist(gen);
        Yaw = dist(gen);

        // health는 건드리지 않음
    }

};