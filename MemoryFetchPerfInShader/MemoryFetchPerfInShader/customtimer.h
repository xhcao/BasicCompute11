#pragma once
#include <Windows.h>

class MandelTimer
{
private:
	// for timing
	LARGE_INTEGER startingTime, endingTime, elapsedMicroseconds;
	LARGE_INTEGER frequency;

public:
	MandelTimer(void)
	{
		QueryPerformanceFrequency(&frequency);
		Start();
	}

	void Start()
	{
		QueryPerformanceCounter(&startingTime);
	}

	// return elapsed time in microseconds since last start
	LONGLONG ElapsedMicroseconds()
	{
		QueryPerformanceCounter(&endingTime);
		elapsedMicroseconds.QuadPart = endingTime.QuadPart - startingTime.QuadPart;
		elapsedMicroseconds.QuadPart *= 1000000;
		elapsedMicroseconds.QuadPart /= frequency.QuadPart;

		return elapsedMicroseconds.QuadPart;
	}

	LONGLONG Stop()
	{
		return ElapsedMicroseconds();
	}
};
