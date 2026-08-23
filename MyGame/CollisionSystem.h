#pragma once
#include "MyECS.h"
#include "Components.h"
#include "Events.h"

class CollisionSystem : public System
{
public:
	virtual void Init(Manager& manager) override
	{
		m_fRange = 32.f;
	}

	virtual void Tick(Manager& manager, float deltatime) override
	{
		for (int i = 0; i < 4; ++i)
		{
			m_bExtendCollided[i] = false;
		}

		m_fColBox[0] = m_Position->m_fPosY + m_Collider->m_fBoxSize[0];
		m_fColBox[1] = m_Position->m_fPosY + m_Collider->m_fBoxSize[1];
		m_fColBox[2] = m_Position->m_fPosX + m_Collider->m_fBoxSize[2];
		m_fColBox[3] = m_Position->m_fPosX + m_Collider->m_fBoxSize[3];

		int ix, iy;
		ix = (int)m_Position->m_fPosX / 64;
		iy = (int)m_Position->m_fPosY / 64;

		for (auto wall : m_Walls)
		{
			auto obs = wall->GetComponent<Obstacle>();
			if (obs->m_iX <= ix + 2 && obs->m_iX >= ix - 2 && obs->m_iY <= iy + 2 && obs->m_iY >= iy - 2)
			{
				CollisionCheckWithWalls(manager, wall);
			}
		}

		for (auto spike : m_Spikes)
		{
			auto obs = spike->GetComponent<Obstacle>();
			if (obs->m_iX <= ix + 2 && obs->m_iX >= ix - 2 && obs->m_iY <= iy + 2 && obs->m_iY >= iy - 2)
			{
				CollisionCheckWithSpikes(manager, spike);
			}
		}

		auto obs = m_Portal->GetComponent<Obstacle>();
		if (obs->m_iX <= ix + 2 && obs->m_iX >= ix - 2 && obs->m_iY <= iy + 2 && obs->m_iY >= iy - 2)
		{
			CollisionCheckWithPortal(manager, m_Portal);
		}

		if (!m_bExtendCollided[0] + !m_bExtendCollided[1] + !m_bExtendCollided[2] + !m_bExtendCollided[3])
		{
			manager.AddEvent<ExtendedCollisionResult>(m_bExtendCollided[0], m_bExtendCollided[1], m_bExtendCollided[2], m_bExtendCollided[3]);
		}
		
	}

	virtual void Received(Manager& manager, OnEntityCreated* event) override
	{
		if (event->m_Entity->IsHavingComponent<Position, Collider>())
		{
			if (event->m_Entity->IsHavingComponent <Player>())
			{
				m_Position = event->m_Entity->GetComponent<Position>();
				m_Collider = event->m_Entity->GetComponent<Collider>();
			}
			else if (event->m_Entity->IsHavingComponent <Obstacle>())
			{
				if(event->m_Entity->GetComponent<Obstacle>())
				{
					if (event->m_Entity->GetComponent<Obstacle>()->m_ObstacleKind == 0)
						m_Walls.push_back(event->m_Entity);
					else if (event->m_Entity->GetComponent<Obstacle>()->m_ObstacleKind == 5)
						m_Portal = event->m_Entity;
				else
					m_Spikes.push_back(event->m_Entity);
				}
			}

		}
	}

	virtual void Received(Manager& manager, OnEntityDestroyed* event) override
	{
		if (event->m_Entity->IsHavingComponent<Position, Collider>())
		{
			if (event->m_Entity->IsHavingComponent <Player>())
			{
				m_Position = nullptr;
				m_Collider = nullptr;
			}
			else if (event->m_Entity->IsHavingComponent <Obstacle>())
			{
				if (event->m_Entity->GetComponent<Obstacle>())
				{
					if (event->m_Entity->GetComponent<Obstacle>()->m_ObstacleKind == 0)
					{
						m_Walls.erase(std::remove(m_Walls.begin(), m_Walls.end(), event->m_Entity));
					}
					else if (event->m_Entity->GetComponent<Obstacle>()->m_ObstacleKind == 5)
					{
						m_Portal = nullptr;
					}
					else
					{
						m_Spikes.erase(std::remove(m_Spikes.begin(), m_Spikes.end(), event->m_Entity));
					}
				}
			}

		}
	}

	void CollisionCheckWithWalls(Manager& manager, Entity* ent)
	{
		int dir = CollisionCheck(manager, ent);
		if (dir >= 0)
		{
			manager.AddEvent<PlayerCollided>(dir);
		}
	}
	void CollisionCheckWithSpikes(Manager& manager, Entity* ent)
	{
		int dir = CollisionCheck(manager, ent);
		int obs = ent->GetComponent<Obstacle>()->m_ObstacleKind;
		if (dir >= 0)
		{
			if (dir == 1 && obs == 1)
			{
				manager.AddEvent<PlayerDamaged>();
			}
			else if (dir == 0 && obs == 2)
			{
				manager.AddEvent<PlayerDamaged>();
			}
			else if (dir == 2 && obs == 4)
			{
				manager.AddEvent<PlayerDamaged>();
			}
			else if (dir == 3 && obs == 3)
			{
				manager.AddEvent<PlayerDamaged>();
			}
			else
			manager.AddEvent<PlayerCollided>(dir);
		}
	}
	void CollisionCheckWithPortal(Manager& manager, Entity* ent)
	{
		int dir = CollisionCheck(manager, ent);
		if (dir >= 0)
		{
			manager.AddEvent<PortalCollided>();
		}
	}

	int CollisionCheck(Manager& manager, Entity* ent)
	{
		auto col = ent->GetComponent<Collider>();
		auto pos = ent->GetComponent<Position>();

		if (m_Collider->m_bIsActive && col->m_bIsActive)
		{
			for (int i = 0; i < 4; ++i)
			{
				m_Collider->m_bCollided[i] = false;
			}

			float top, bot, left, right;

			top = pos->m_fPosY + col->m_fBoxSize[0];
			bot = pos->m_fPosY + col->m_fBoxSize[1];
			left = pos->m_fPosX + col->m_fBoxSize[2];
			right = pos->m_fPosX + col->m_fBoxSize[3];

			if (m_fColBox[0] < bot &&
				m_fColBox[1] > top &&
				m_fColBox[2] < right &&
				m_fColBox[3] > left)
			{
				//Check
				if (bot - m_fColBox[0] < m_fRange && bot - m_fColBox[0] > 0.f && col->m_bCanCollision[1]) // 위
				{
					m_Collider->m_bCollided[0] = true;
				}
				if (m_fColBox[1] - top < m_fRange && m_fColBox[1] - top > 0.f && col->m_bCanCollision[0]) // 아래
				{
					m_Collider->m_bCollided[1] = true;
				}
				if (right - m_fColBox[2] < m_fRange && right - m_fColBox[2] > 0.f && col->m_bCanCollision[3]) // 왼쪽
				{
					m_Collider->m_bCollided[2] = true;
				}
				if (m_fColBox[3] - left < m_fRange && m_fColBox[3] - left > 0.f && col->m_bCanCollision[2]) // 오른쪽
				{
					m_Collider->m_bCollided[3] = true;
				}

				if (m_Collider->m_bCollided[0] + m_Collider->m_bCollided[1] + m_Collider->m_bCollided[2] + m_Collider->m_bCollided[3] > 1) // 2방향 충돌시
				{
					if (m_Collider->m_bCollided[0])
					{
						if (m_Collider->m_bCollided[2])
						{
							if (bot - m_fColBox[0] > right - m_fColBox[2])
								m_Collider->m_bCollided[0] = false;
							else
								m_Collider->m_bCollided[2] = false;
						}
						else
						{
							if (bot - m_fColBox[0] > m_fColBox[3] - left)
								m_Collider->m_bCollided[0] = false;
							else
								m_Collider->m_bCollided[3] = false;
						}
					}
					else
					{
						if (m_Collider->m_bCollided[2])
						{
							if (m_fColBox[1] - top > right - m_fColBox[2])
								m_Collider->m_bCollided[1] = false;
							else
								m_Collider->m_bCollided[2] = false;
						}
						else
						{
							if (m_fColBox[1] - top > m_fColBox[3] - left)
								m_Collider->m_bCollided[1] = false;
							else
								m_Collider->m_bCollided[3] = false;
						}
					}
				}
				
					if (m_Collider->m_bCollided[0])
					{
						m_Position->m_fPosY = bot - m_Collider->m_fBoxSize[0];
						m_fColBox[0] = m_Position->m_fPosY + m_Collider->m_fBoxSize[0];
						m_fColBox[1] = m_Position->m_fPosY + m_Collider->m_fBoxSize[1];
					}
					else if (m_Collider->m_bCollided[1])
					{
						m_Position->m_fPosY = top - m_Collider->m_fBoxSize[1];
						m_fColBox[0] = m_Position->m_fPosY + m_Collider->m_fBoxSize[0];
						m_fColBox[1] = m_Position->m_fPosY + m_Collider->m_fBoxSize[1];
					}
					else if (m_Collider->m_bCollided[2])
					{
						m_Position->m_fPosX = right - m_Collider->m_fBoxSize[2];
						m_fColBox[2] = m_Position->m_fPosX + m_Collider->m_fBoxSize[2];
						m_fColBox[3] = m_Position->m_fPosX + m_Collider->m_fBoxSize[3];
					}
					else if (m_Collider->m_bCollided[3])
					{
						m_Position->m_fPosX = left - m_Collider->m_fBoxSize[3];
						m_fColBox[2] = m_Position->m_fPosX + m_Collider->m_fBoxSize[2];
						m_fColBox[3] = m_Position->m_fPosX + m_Collider->m_fBoxSize[3];
					}
				
			}


			// 연장선 체크

			if (m_fColBox[0] - 10.f < bot &&
				m_fColBox[1] > top &&
				m_fColBox[2] < right &&
				m_fColBox[3] > left)
			{
				m_bExtendCollided[0] = true;
			}
			if (m_fColBox[0] < bot &&
				m_fColBox[1] +10.f > top &&
				m_fColBox[2] < right &&
				m_fColBox[3] > left)
			{
				m_bExtendCollided[1] = true;
			}
			if (m_fColBox[0] < bot &&
				m_fColBox[1] > top &&
				m_fColBox[2] -10.f < right &&
				m_fColBox[3] > left)
			{
				m_bExtendCollided[2] = true;
			}
			if (m_fColBox[0] < bot &&
				m_fColBox[1] > top &&
				m_fColBox[2] < right &&
				m_fColBox[3] +10.f > left)
			{
				m_bExtendCollided[3] = true;
			}


			for (int i = 0; i < 4; ++i)
			{
				if (m_Collider->m_bCollided[i])
				{
					return i;
				}
			}
			
		}
		
		return -1;
	}
private:

	Position* m_Position;
	Collider* m_Collider;
	float m_fColBox[4];

	bool m_bExtendCollided[4];

	std::vector<Entity*> m_Walls;
	std::vector<Entity*> m_Spikes;
	Entity* m_Portal;

	float m_fRange;
};
