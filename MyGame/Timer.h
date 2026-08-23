#pragma once
#include "Definitions.h"

class Timer
{
public:
	Timer()
	{
		Reset();
	}

	~Timer() = default;

	void Reset()
	{
		QueryPerformanceFrequency(&m_CountTime);
		QueryPerformanceCounter(&m_CurTime);
		QueryPerformanceCounter(&m_PrevTime);
	}

	double Update()
	{
		QueryPerformanceCounter(&m_CurTime);

		// delta time = current time - previous time
		m_dDeltaTime = (static_cast<double>(m_CurTime.QuadPart) - static_cast<double>(m_PrevTime.QuadPart)) / static_cast<double>(m_CountTime.QuadPart);

		//m_fDeltaTime = static_cast<float>(m_dDeltaTime);
		m_PrevTime = m_CurTime;

		return m_dDeltaTime;
	}

private:
	LARGE_INTEGER m_CountTime;
	LARGE_INTEGER m_CurTime;
	LARGE_INTEGER m_PrevTime;
	double m_dDeltaTime;
	float m_fDeltaTime;
};