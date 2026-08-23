#pragma once
#include "Definitions.h"

struct AnimationEnd : IEvent
{
	AnimationEnd(std::string anim) : m_Animation(anim) {};
	std::string m_Animation;
};

struct PlayerDamaged : IEvent
{

};


struct PlayerCollided : IEvent
{
	PlayerCollided(int dir)
	{
		m_iDir = dir;
	}
	int m_iDir;
};

struct ExtendedCollisionResult : IEvent
{
	ExtendedCollisionResult(bool r1, bool r2, bool r3, bool r4)
	{
		m_bCollided[0] = r1;
		m_bCollided[1] = r2;
		m_bCollided[2] = r3;
		m_bCollided[3] = r4;
	}
	bool m_bCollided[4];
};

struct SceneChange : IEvent
{
	SceneChange(int is)
	{
		m_iScene = is;
	}

	int m_iScene;
};

struct PortalCollided : IEvent
{

};

struct ChangeStage : IEvent
{
	ChangeStage(int stage = -1)
	{
		m_iStage = stage;
	}

	int m_iStage;
};

struct PlayerRespawn : IEvent
{

};

struct PlayBGM : IEvent
{
	PlayBGM(int i, bool b)
	{
		m_iKind = i;
		m_bPlay = b;
	}
	int m_iKind;
	bool m_bPlay;
};

struct PlaySFX : IEvent
{
	PlaySFX(int i)
	{
		m_iKind = i;
	}
	int m_iKind;
};

struct InputEvent : IEvent
{
	InputEvent(std::string s)
	{
		m_Input = s;
	}
	std::string m_Input;
};
