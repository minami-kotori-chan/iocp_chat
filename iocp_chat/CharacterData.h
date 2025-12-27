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

    UINT8 State=0;

    CharacterData()
    {
        SetRandomForTest();
    }
    void SetRandomForTest()
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(1.0f, 100.0f);

        x = dist(gen);
        y = dist(gen);
        z = dist(gen);
        Yaw = dist(gen);

        // health는 건드리지 않음
    }

};