#pragma once
#include "MyECS.h"
#include "Components.h"
#include "Events.h"

#include "JsonReader.h"

class StageSystem : public System,
	public EventSubscriber<ChangeStage>
{
public:
	virtual void Init(Manager& manager) override
	{
		manager.SubscribeEvent<StageSystem, ChangeStage>();

		mapInfos.push_back(MapInfo::GetFromFile("Data/Stage/Stage1.json"));
		mapInfos.push_back(MapInfo::GetFromFile("Data/Stage/Stage2.json"));
		mapInfos.push_back(MapInfo::GetFromFile("Data/Stage/Stage3.json"));

		m_iCurrentStage = -1;
	}
	virtual void Tick(Manager& manager, float deltatime) override
	{
	}

	virtual void Received(Manager& manager, ChangeStage* event) override
	{
		if (event->m_iStage == -1)
		{
			if (m_iCurrentStage == 4)
				m_iCurrentStage = 0;
			else
			m_iCurrentStage++;
		}
		else
		{
			if (m_iCurrentStage == event->m_iStage)
				return;
			m_iCurrentStage = event->m_iStage;
		}
		
		manager.ClearEntity();

		if(event->m_iStage == 0 || event->m_iStage == 4 || m_iCurrentStage == 0 || m_iCurrentStage == 4)
		{
			manager.InactivateSystem<CollisionSystem>();
			manager.InactivateSystem<PlayerUpdateSystem>();
			manager.InactivateSystem<AnimationSystem>();
			manager.CreateEntity([&](Entity* ent)
			{
				ent->AddComponent<Position>(800.f, 500.f);
				ent->AddComponent<Sprite>("Title");
			});
		}

		else 
		{
			manager.ActivateSystem<CollisionSystem>();
			manager.ActivateSystem<PlayerUpdateSystem>();
			manager.ActivateSystem<AnimationSystem>();
			mapInfo = mapInfos[m_iCurrentStage-1];


			manager.CreateEntity([&](Entity* ent)
			{
				ent->AddComponent<Position>(1000.f, 1000.f);
				ent->AddComponent<Sprite>("Background");
			});

			std::vector<std::vector<int>> map(mapInfo.height, std::vector<int>(mapInfo.width, 0));

			for (int y = 0; y < mapInfo.height; ++y)
			{
				for (int x = 0; x < mapInfo.width; ++x)
				{
					map[y][x] = mapInfo.data[x + y * mapInfo.width];
				}
			}

			for (int y = 0; y < mapInfo.height; ++y)
			{
				for (int x = 0; x < mapInfo.width; ++x)
				{
					if (map[y][x] == 1) // Wall
					{
						manager.CreateEntity([&](Entity* ent)
						{
							ent->AddComponent<Position>(64.f * x, 64.f * y);
							ent->AddComponent<Sprite>("Block", 1.28f, 1.28f);
							ent->AddComponent<Obstacle>(0, x, y);
							bool dir[4] = { true, true, true, true };
							if (y > 0 && map[y - 1][x] == 1)
								dir[0] = false;
							if (y < mapInfo.height - 1 && map[y + 1][x] == 1)
								dir[1] = false;
							if (x > 0 && map[y][x - 1] == 1)
								dir[2] = false;
							if (x < mapInfo.width - 1 && map[y][x + 1] == 1)
								dir[3] = false;
							ent->AddComponent<Collider>(ent->GetComponent<Sprite>());
							for (int i = 0; i < 4; ++i)
							{
								ent->GetComponent<Collider>()->m_bCanCollision[i] = dir[i];
							}
						});
					}

					if (map[y][x] == 2) // Spike Up
					{
						manager.CreateEntity([&](Entity* ent)
						{
							ent->AddComponent<Position>(64.f * x, 64.f *y);
							ent->AddComponent<Rotation>(0.f);
							ent->AddComponent<Sprite>("Spike", 1.28f, 1.28f);
							ent->AddComponent<Obstacle>(1, x, y);
							ent->AddComponent<Collider>(ent->GetComponent<Sprite>());
						});
					}
					if (map[y][x] == 3) // Spike Down
					{
						manager.CreateEntity([&](Entity* ent)
						{
							ent->AddComponent<Position>(64.f * x, 64.f *y);
							ent->AddComponent<Rotation>(180.f);
							ent->AddComponent<Sprite>("Spike", 1.28f, 1.28f);
							ent->AddComponent<Obstacle>(2, x, y);
							ent->AddComponent<Collider>(ent->GetComponent<Sprite>());
						});
					}
					if (map[y][x] == 4) // Spike Left
					{
						manager.CreateEntity([&](Entity* ent)
						{
							ent->AddComponent<Position>(64.f * x, 64.f *y);
							ent->AddComponent<Rotation>(-90.f);
							ent->AddComponent<Sprite>("Spike", 1.28f, 1.28f);
							ent->AddComponent<Obstacle>(3, x, y);
							ent->AddComponent<Collider>(ent->GetComponent<Sprite>());
						});
					}
					if (map[y][x] == 5) // Spike Right
					{
						manager.CreateEntity([&](Entity* ent)
						{
							ent->AddComponent<Position>(64.f * x, 64.f *y);
							ent->AddComponent<Rotation>(90.f);
							ent->AddComponent<Sprite>("Spike", 1.28f, 1.28f);
							ent->AddComponent<Obstacle>(4, x, y);
							ent->AddComponent<Collider>(ent->GetComponent<Sprite>());
						});
					}
					if (map[y][x] == 6) // Player
					{
						manager.CreateEntity([&](Entity* ent)
						{
							ent->AddComponent<Position>(64.f * x, 64.f * y);
							ent->AddComponent<Velocity>();
							ent->AddComponent<Rotation>();
							ent->AddComponent<Sprite>("Move", 2.f, 2.f);
							ent->AddComponent<Animation>(ent->GetComponent<Sprite>());
							ent->AddComponent<Collider>(ent->GetComponent<Sprite>(), 0.7f, 0.7f, 0.f, -7.f);
							ent->AddComponent<Player>(64.f * x, 64.f * y);
						});
					}

					if (map[y][x] == 7) // Portal
					{
						manager.CreateEntity([&](Entity* ent)
						{
							ent->AddComponent<Position>(64.f * x, 64.f * y);
							ent->AddComponent<Sprite>("Portal", 1.7f, 1.5f);
							ent->AddComponent<Animation>(ent->GetComponent<Sprite>());
							ent->AddComponent<Collider>(ent->GetComponent<Sprite>(), 0.1f, 0.1f, 0.f, 0.f);
							ent->AddComponent<Obstacle>(5, x, y);
						});
					}
				}
			}

		}
	}

	private:
		int m_iCurrentStage;
		std::vector<MapInfo> mapInfos;
		MapInfo mapInfo;
};