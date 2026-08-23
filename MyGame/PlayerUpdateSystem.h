#pragma once
#include "MyECS.h"
#include "Components.h"
#include "Events.h"

class PlayerUpdateSystem : public System,
	public EventSubscriber<PlayerCollided>,
	public EventSubscriber<ExtendedCollisionResult>,
	public EventSubscriber<AnimationEnd>,
	public EventSubscriber<PlayerDamaged>,
	public EventSubscriber<PortalCollided>,
	public EventSubscriber<InputEvent>

{
public:
	virtual void Init(Manager& manager) override
	{
		manager.SubscribeEvent<PlayerUpdateSystem, PlayerCollided>();
		manager.SubscribeEvent<PlayerUpdateSystem, ExtendedCollisionResult>();
		manager.SubscribeEvent<PlayerUpdateSystem, AnimationEnd>();
		manager.SubscribeEvent<PlayerUpdateSystem, PlayerDamaged>();
		manager.SubscribeEvent<PlayerUpdateSystem, PortalCollided>();
		manager.SubscribeEvent<PlayerUpdateSystem, InputEvent>();
		
		m_bIsDead = false;
		m_bIsCleared = false;
	}

	virtual void Tick(Manager& manager, float deltatime) override
	{
		if (m_PlayerCom->m_bStop)
		{
			if (m_PlayerCom->m_PlayerState == PlayerState::RUN)
				ChangeState(PlayerState::IDLE);
		}
		else
		{
			if (m_PlayerCom->m_PlayerState == PlayerState::IDLE)
				ChangeState(PlayerState::RUN);

		}
		if (m_PlayerCom->m_bInAir)
		{
			if (m_PlayerCom->m_PlayerState == PlayerState::RUN)
				ChangeState(PlayerState::JUMP);
		}

		switch (m_PlayerCom->m_GravityDirection)
		{
		case GravityDirection::DOWN:
			if (m_PlayerCom->m_bStop)
			{
				m_Velocity->m_fVelX = 0.f;
			}
			else
			{
				m_Velocity->m_fVelX = m_Velocity->m_fRunSpeed;
			}
			if (m_PlayerCom->m_bInAir)
			{
				m_Velocity->m_fVelY -= m_Velocity->m_fGravityScale * deltatime;
				if (m_Velocity->m_fVelY < -400.f)
				{
					m_Velocity->m_fVelY = -400.f;
				}
			}

			break;
		case GravityDirection::RIGHT:
			if (m_PlayerCom->m_bStop)
			{
				m_Velocity->m_fVelY = 0.f;
			}
			else
			{
				m_Velocity->m_fVelY = m_Velocity->m_fRunSpeed;
			}
			if (m_PlayerCom->m_bInAir)
			{
				m_Velocity->m_fVelX += m_Velocity->m_fGravityScale * deltatime;
				if (m_Velocity->m_fVelX > 400.f)
				{
					m_Velocity->m_fVelX = 400.f;
				}
			}

			break;
		case GravityDirection::UP:
			if (m_PlayerCom->m_bStop)
			{
				m_Velocity->m_fVelX = 0.f;
			}
			else
			{
				m_Velocity->m_fVelX = -m_Velocity->m_fRunSpeed;
			}
			if (m_PlayerCom->m_bInAir)
			{
				m_Velocity->m_fVelY += m_Velocity->m_fGravityScale * deltatime;
				if (m_Velocity->m_fVelY > 400.f)
				{
					m_Velocity->m_fVelY = 400.f;
				}
			}

			break;
		case GravityDirection::LEFT:
			if (m_PlayerCom->m_bStop)
			{
				m_Velocity->m_fVelY = 0.f;
			}
			else
			{
				m_Velocity->m_fVelY = -m_Velocity->m_fRunSpeed;
			}
			if (m_PlayerCom->m_bInAir)
			{
				m_Velocity->m_fVelX -= m_Velocity->m_fGravityScale * deltatime;
				if (m_Velocity->m_fVelX < -400.f)
				{
					m_Velocity->m_fVelX = -400.f;
				}
			}

			break;
		}

		if (m_PlayerCom->m_PlayerState != PlayerState::DEATH)
		{
			if (m_bIsCleared)
			{
				m_fAccTime += deltatime;
				if (m_fAccTime > m_PlayerCom->m_fWaitTime * 0.3f)
				{
					m_Sprite->m_bIsActive = false;
				}
				if (m_fAccTime > m_PlayerCom->m_fWaitTime)
				{
					m_bIsCleared = false;
					m_fAccTime = 0.f;
					manager.AddEvent<ChangeStage>();
				}
			}
			else
			{
				m_Position->m_fPosX += m_Velocity->m_fVelX * deltatime;
				m_Position->m_fPosY -= m_Velocity->m_fVelY * deltatime;
			}
		}
		else
		{
			if (m_bIsDead)
				m_fAccTime += deltatime;
			if (m_fAccTime > m_PlayerCom->m_fWaitTime * 0.5f)
			{
				ChangeState(PlayerState::IDLE);
				m_Position->m_fPosX = m_PlayerCom->m_fInitPosX;
				m_Position->m_fPosY = m_PlayerCom->m_fInitPosY;
				ChangeGravity(m_PlayerCom->m_GravityDirection, GravityDirection::DOWN);
				m_PlayerCom->m_bStop = false;
				m_PlayerCom->m_bInAir = false;
				m_Velocity->m_fVelX = 0.f;
				m_Velocity->m_fVelY = 0.f;
				m_bIsDead = false;
				m_fAccTime = 0.f;
				manager.AddEvent<PlayerRespawn>();
			}
		}


	}

	virtual void Received(Manager& manager, OnEntityCreated* event) override
	{
		if (event->m_Entity->GetComponent<Player>())
		{
			m_Player = event->m_Entity;
			m_PlayerCom = m_Player->GetComponent<Player>();
			m_Position = m_Player->GetComponent<Position>();
			m_Velocity = m_Player->GetComponent<Velocity>();
			m_Rotation = m_Player->GetComponent<Rotation>();
			m_Sprite = m_Player->GetComponent<Sprite>();
			m_Collider = m_Player->GetComponent<Collider>();
			m_Animation = m_Player->GetComponent<Animation>();
		}
	}

	virtual void Received(Manager& manager, OnEntityDestroyed* event) override
	{
		if (event->m_Entity->GetComponent<Player>())
		{
			m_Player = nullptr;
			m_PlayerCom = nullptr;
			m_Position = nullptr;
			m_Velocity = nullptr;
			m_Rotation = nullptr;
			m_Sprite = nullptr;
			m_Collider = nullptr;
			m_Animation = nullptr;
		}
	}

	virtual void Received(Manager& manager, PlayerCollided* event) override
	{
		switch (event->m_iDir)
		{
		case 0:
			if (m_PlayerCom->m_GravityDirection == GravityDirection::UP)
			{
				m_PlayerCom->m_bInAir = false;
				if (m_PlayerCom->m_PlayerState == PlayerState::JUMP)
				{
				m_Velocity->m_fVelY = 0.f;
					ChangeState(PlayerState::RUN);
				}
			}
			if (m_PlayerCom->m_GravityDirection == GravityDirection::RIGHT)
			{
				m_PlayerCom->m_bStop = true;
			}
			if (m_PlayerCom->m_GravityDirection == GravityDirection::DOWN)
			{
				if (m_PlayerCom->m_PlayerState == PlayerState::JUMP)
				{
					m_Velocity->m_fVelY = -100.f;
				}
				else if (m_PlayerCom->m_PlayerState == PlayerState::SPIN)
				{
					m_Velocity->m_fVelY = 100.f;
				}
			}
			break;
		case 1:
			if (m_PlayerCom->m_GravityDirection == GravityDirection::DOWN)
			{
				m_PlayerCom->m_bInAir = false;
				if (m_PlayerCom->m_PlayerState == PlayerState::JUMP)
				{
					m_Velocity->m_fVelY = 0.f;
					ChangeState(PlayerState::RUN);
				}
			}
			if (m_PlayerCom->m_GravityDirection == GravityDirection::LEFT)
			{
				m_PlayerCom->m_bStop = true;
			}
			if (m_PlayerCom->m_GravityDirection == GravityDirection::UP)
			{
				if (m_PlayerCom->m_PlayerState == PlayerState::JUMP)
				{
					m_Velocity->m_fVelY = 100.f;
				}
				else if (m_PlayerCom->m_PlayerState == PlayerState::SPIN)
				{
					m_Velocity->m_fVelY = -100.f;
				}
			}
			break;
		case 2:
			if (m_PlayerCom->m_GravityDirection == GravityDirection::LEFT)
			{
				m_PlayerCom->m_bInAir = false;
				if (m_PlayerCom->m_PlayerState == PlayerState::JUMP)
				{
				m_Velocity->m_fVelX = 0.f;
					ChangeState(PlayerState::RUN);
				}
			}
			if (m_PlayerCom->m_GravityDirection == GravityDirection::UP)
			{
				m_PlayerCom->m_bStop = true;
			}
			if (m_PlayerCom->m_GravityDirection == GravityDirection::RIGHT)
			{
				if (m_PlayerCom->m_PlayerState == PlayerState::JUMP)
				{
					m_Velocity->m_fVelX = 100.f;
				}
				else if (m_PlayerCom->m_PlayerState == PlayerState::SPIN)
				{
					m_Velocity->m_fVelX = -100.f;
				}
			}
			break;
		case 3:
			if (m_PlayerCom->m_GravityDirection == GravityDirection::RIGHT)
			{
				m_PlayerCom->m_bInAir = false;
				if (m_PlayerCom->m_PlayerState == PlayerState::JUMP)
				{
				m_Velocity->m_fVelX = 0.f;
					ChangeState(PlayerState::RUN);
				}
			}
			if (m_PlayerCom->m_GravityDirection == GravityDirection::DOWN)
			{
				m_PlayerCom->m_bStop = true;
			}
			if (m_PlayerCom->m_GravityDirection == GravityDirection::LEFT)
			{
				if (m_PlayerCom->m_PlayerState == PlayerState::JUMP)
				{
					m_Velocity->m_fVelX = -100.f;
				}
				else if (m_PlayerCom->m_PlayerState == PlayerState::SPIN)
				{
					m_Velocity->m_fVelX = 100.f;
				}
			}
			break;
		}
	}
	virtual void Received(Manager& manager, ExtendedCollisionResult* event) override
	{
		switch (m_PlayerCom->m_GravityDirection)
		{
		case GravityDirection::UP:
			if (!event->m_bCollided[0])
			{
				m_PlayerCom->m_bInAir = true;
			}
			if (!event->m_bCollided[2])
			{
				m_PlayerCom->m_bStop = false;
			}
			break;
		case GravityDirection::DOWN:
			if (!event->m_bCollided[1])
			{
				m_PlayerCom->m_bInAir = true;
			}
			if (!event->m_bCollided[3])
			{
				m_PlayerCom->m_bStop = false;
			}
			break;
		case GravityDirection::LEFT:
			if (!event->m_bCollided[2])
			{
				m_PlayerCom->m_bInAir = true;
			}
			if (!event->m_bCollided[1])
			{
				m_PlayerCom->m_bStop = false;
			}
			break;
		case GravityDirection::RIGHT:
			if (!event->m_bCollided[3])
			{
				m_PlayerCom->m_bInAir = true;
			}
			if (!event->m_bCollided[0])
			{
				m_PlayerCom->m_bStop = false;
			}
			break;
		}
	}

	virtual void Received(Manager& manager, AnimationEnd* event) override
	{
		if (event->m_Animation == "Roll")
		{
			if (m_PlayerCom->m_bStop)
			{
				ChangeState(PlayerState::RUN);
				m_PlayerCom->m_bInAir = false;
			}
			else
			{
				ChangeState(PlayerState::JUMP);
				m_PlayerCom->m_bInAir = true;
			}
			
			switch (m_PlayerCom->m_GravityDirection)
			{
			case GravityDirection::UP:
				ChangeGravity(m_PlayerCom->m_GravityDirection, GravityDirection::LEFT);
				break;
			case GravityDirection::DOWN:
				ChangeGravity(m_PlayerCom->m_GravityDirection, GravityDirection::RIGHT);
				break;
			case GravityDirection::LEFT:
				ChangeGravity(m_PlayerCom->m_GravityDirection, GravityDirection::DOWN);
				break;
			case GravityDirection::RIGHT:
				ChangeGravity(m_PlayerCom->m_GravityDirection, GravityDirection::UP);
				break;
			}

		}

		if (event->m_Animation == "Death")
		{
			m_bIsDead = true;
		}
	}

	virtual void Received(Manager& manager, PlayerDamaged* event) override
	{
		ChangeState(PlayerState::DEATH);
		m_fAccTime = 0.f;
	}

	virtual void Received(Manager& manager, PortalCollided* event) override
	{
		if (m_bIsCleared == false)
		{
			m_bIsCleared = true;
			manager.CreateEntity([&](Entity* ent)
			{
				ent->AddComponent<Position>(m_Position->m_fPosX, m_Position->m_fPosY);
				ent->AddComponent<Sprite>("PortalAnimation");
				ent->AddComponent<Animation>(ent->GetComponent<Sprite>(), false);
				ent->GetComponent<Animation>()->m_fAnimSpeed = 20.f;
			});
		}
	}

	virtual void Received(Manager& manager, InputEvent* event) override
	{
		if (event->m_Input == "X")
		{
			if (m_PlayerCom->m_PlayerState == PlayerState::RUN || m_PlayerCom->m_PlayerState == PlayerState::IDLE)
			{
				if (ChangeState(PlayerState::JUMP))
				{
					manager.AddEvent<PlaySFX>(0);
				}
				switch (m_PlayerCom->m_GravityDirection)
				{
				case GravityDirection::UP:
					m_Velocity->m_fVelY = -m_Velocity->m_fJumpSpeed;
					break;
				case GravityDirection::DOWN:
					m_Velocity->m_fVelY = m_Velocity->m_fJumpSpeed;
					break;
				case GravityDirection::LEFT:
					m_Velocity->m_fVelX = m_Velocity->m_fJumpSpeed;
					break;
				case GravityDirection::RIGHT:
					m_Velocity->m_fVelX = -m_Velocity->m_fJumpSpeed;
					break;
				}

			}
		}
		if (event->m_Input == "Z")
		{
			if (m_PlayerCom->m_PlayerState == PlayerState::RUN || m_PlayerCom->m_PlayerState == PlayerState::IDLE)
			{
				if (ChangeState(PlayerState::SPIN))
				{
					manager.AddEvent<PlaySFX>(1);
				}
				switch (m_PlayerCom->m_GravityDirection)
				{
				case GravityDirection::UP:
					m_Velocity->m_fVelY = -m_Velocity->m_fJumpSpeed;
					break;
				case GravityDirection::DOWN:
					m_Velocity->m_fVelY = m_Velocity->m_fJumpSpeed;
					break;
				case GravityDirection::LEFT:
					m_Velocity->m_fVelX = m_Velocity->m_fJumpSpeed;
					break;
				case GravityDirection::RIGHT:
					m_Velocity->m_fVelX = -m_Velocity->m_fJumpSpeed;
					break;
				}
			}
		}

	}

	void ChangeGravity(GravityDirection src, GravityDirection dst)
	{
		if (src == dst)
			return;
		m_PlayerCom->m_GravityDirection = dst;

		switch (src)
		{
		case GravityDirection::UP: // 컬리전 박스 바꿔주는 작업
			m_Collider->m_fBoxSize[0] += 7.f;
			m_Collider->m_fBoxSize[1] += 7.f;
			break;
		case GravityDirection::DOWN:
			m_Collider->m_fBoxSize[0] -= 7.f;
			m_Collider->m_fBoxSize[1] -= 7.f;
			break;
		case GravityDirection::LEFT:
			m_Collider->m_fBoxSize[2] += 7.f;
			m_Collider->m_fBoxSize[3] += 7.f;
			break;
		case GravityDirection::RIGHT:
			m_Collider->m_fBoxSize[2] -= 7.f;
			m_Collider->m_fBoxSize[3] -= 7.f;
			break;
		}

		switch (dst)
		{
		case GravityDirection::UP:
			m_Collider->m_fBoxSize[0] -= 7.f;
			m_Collider->m_fBoxSize[1] -= 7.f;
			m_Rotation->m_fRotZ = 180.f;
			break;
		case GravityDirection::DOWN:
			m_Collider->m_fBoxSize[0] += 7.f;
			m_Collider->m_fBoxSize[1] += 7.f;
			m_Rotation->m_fRotZ = 0.f;
			break;
		case GravityDirection::LEFT:
			m_Collider->m_fBoxSize[2] -= 7.f;
			m_Collider->m_fBoxSize[3] -= 7.f;
			m_Rotation->m_fRotZ = 90.f;
			break;
		case GravityDirection::RIGHT:
			m_Collider->m_fBoxSize[2] += 7.f;
			m_Collider->m_fBoxSize[3] += 7.f;
			m_Rotation->m_fRotZ = -90.f;
			break;
		}
	}

	bool ChangeState(PlayerState dst)
	{
		if (m_PlayerCom->m_PlayerState == dst)
			return false;
		m_PlayerCom->m_PlayerState = dst;
		switch (dst)
		{
		case PlayerState::IDLE:
			m_Animation->m_SpriteState = "Idle";
			m_Animation->m_fCurIndex = 0.f;
			m_Animation->m_bLoop = true;
			break;
		case PlayerState::RUN:
			m_Animation->m_SpriteState = "Move";
			m_Animation->m_fCurIndex = 0.f;
			m_Animation->m_bLoop = true;
			break;
		case PlayerState::JUMP:
			m_Animation->m_SpriteState = "Jump";
			m_Animation->m_fCurIndex = 0.f;
			m_Animation->m_bLoop = true;
			break;
		case PlayerState::SPIN:
			m_Animation->m_SpriteState = "Roll";
			m_Animation->m_fCurIndex = 0.f;
			m_Animation->m_bLoop = false;
			break;
		case PlayerState::FALL:
			m_Animation->m_SpriteState = "Jump";
			m_Animation->m_fCurIndex = 0.f;
			m_Animation->m_bLoop = true;
			break;
		case PlayerState::DEATH:
			m_Animation->m_SpriteState = "Death";
			m_Animation->m_fCurIndex = 0.f;
			m_Animation->m_bLoop = false;
			break;
		}
		return true;
	}

private:
	Entity* m_Player;
	Player* m_PlayerCom;
	Position* m_Position;
	Velocity* m_Velocity;
	Rotation* m_Rotation;
	Sprite* m_Sprite;
	Collider* m_Collider;
	Animation* m_Animation;

	float m_fAccTime;
	bool m_bIsDead;
	bool m_bIsCleared;
};