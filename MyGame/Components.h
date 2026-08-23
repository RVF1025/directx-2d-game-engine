#pragma once
#include "Definitions.h"
#include "Enumerates.h"
#include "GraphicManager.h"
#include "TextureManager.h"

#include <string>

struct Position : IComponent
{
	Position(float x = 0.f, float y = 0.f) 
	{
		m_fPosX = x;
		m_fPosY = y;
	};

	float m_fPosX;
	float m_fPosY;
};

struct Velocity : IComponent
{
	Velocity(float vx = 0.f, float vy = 0.f)
	{
		m_fVelX = vx;
		m_fVelY = vy;
		m_fRunSpeed = 175.f;
		m_fJumpSpeed = 550.f;
		m_fGravityScale = 1000.f;
	}

	float m_fVelX;
	float m_fVelY;
	float m_fRunSpeed;
	float m_fJumpSpeed;
	float m_fGravityScale;
};

struct Rotation : IComponent
{
	Rotation(float rz = 0.f)
	{
		m_fRotZ = rz;
	}

	float m_fRotZ;
};

struct Sprite : IComponent
{
	Sprite(std::string name, float scalex = 1.f, float scaley = 1.f)
	{
		m_bIsActive = true;
		m_FileName = name;
		
		m_fScaleX = scalex;
		m_fScaleY = scaley;
		
		m_bFlip = false;

		auto handle = TextureManager::GetInstance()->GetTexture(m_FileName);
		if (handle)
		{
			m_iWidth = handle->width;
			m_iHeight = handle->height;
		}
	}

	std::string m_FileName;

	bool m_bIsActive;
	bool m_bFlip;
	
	float m_fScaleX;
	float m_fScaleY;

	int m_iWidth;
	int m_iHeight;
};

struct Animation : IComponent
{
	Animation(Sprite* sp, bool loop = true)
	{
		m_SpriteState = sp->m_FileName;
		m_bLoop = loop;

		m_fCurIndex = 0.f;
		m_iEndIndex = 0;
		m_fAnimSpeed = 10.f;
	}
	
	bool m_bLoop;

	std::string m_SpriteState;

	float m_fCurIndex;
	int m_iEndIndex;
	float m_fAnimSpeed;
};

struct Collider : IComponent
{
	Collider(Sprite* sp, float sx = 1.f, float sy = 1.f , float tx = 0.f, float ty = 0.f) 
	{
		m_bIsActive = true;
	
		m_bCollided[0] = false;
		m_bCollided[1] = false;
		m_bCollided[2] = false;
		m_bCollided[3] = false;

		m_fBoxSize[0] = sp->m_iHeight * sp->m_fScaleY * sy * -0.5f - ty;
		m_fBoxSize[1] = sp->m_iHeight * sp->m_fScaleY * sy * 0.5f - ty;
		m_fBoxSize[2] = sp->m_iWidth * sp->m_fScaleX * sx * -0.5f + tx;
		m_fBoxSize[3] = sp->m_iWidth * sp->m_fScaleX * sx * 0.5f + tx;

		m_bCanCollision[0] = true;
		m_bCanCollision[1] = true;
		m_bCanCollision[2] = true;
		m_bCanCollision[3] = true;
	};

	bool m_bIsActive;
	bool m_bCollided[4];
	bool m_bCanCollision[4];

	float m_fBoxSize[4];
};

struct Player : IComponent
{
	Player(float ix, float iy, PlayerState ps = PlayerState::IDLE)
	{
		m_fInitPosX = ix;
		m_fInitPosY = iy;
		m_PlayerState = ps;
		m_GravityDirection = GravityDirection::DOWN;
		m_bStop = false;
		m_bInAir = false;
		m_fWaitTime = 2.f;
	}
	PlayerState m_PlayerState;
	GravityDirection m_GravityDirection;

	float m_fInitPosX;
	float m_fInitPosY;

	bool m_bStop;
	bool m_bInAir;

	float m_fWaitTime;
};

struct Obstacle : IComponent
{

	Obstacle(int kind, int ix, int iy)
	{
		m_ObstacleKind = kind;
		m_iX = ix;
		m_iY = iy;
	}

	//종류
	// 0 벽 / 1 가시 / 2 방향전환하는벽, 플랫폼, 출구, 등등
	int m_ObstacleKind;
	int m_iX;
	int m_iY;
};