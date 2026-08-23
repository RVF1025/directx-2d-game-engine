#pragma once
#include "MyECS.h"
#include "Components.h"
#include "Events.h"

class AnimationSystem : public System
{
public:
	virtual void Init(Manager& manager) override
	{

	}

	virtual void Tick(Manager& manager, float deltatime) override
	{
		manager.Each<Sprite, Animation>([&](Entity* ent, Sprite* sp, Animation* ani)
		{
			ani->m_iEndIndex = TextureManager::GetInstance()->GetMaxIndex(ani->m_SpriteState);
			
					ani->m_fCurIndex += ani->m_fAnimSpeed * deltatime;
				if ((int)ani->m_fCurIndex > ani->m_iEndIndex)
				{
					if (ani->m_bLoop)
					{
						ani->m_fCurIndex = 0.f;
					}
					else
					{
						if (ani->m_SpriteState == "Roll")
						{
							manager.AddEvent<AnimationEnd>("Roll");
						}
						else if (ani->m_SpriteState == "Death")
						{
							manager.AddEvent<AnimationEnd>("Death");
						}
						
						else
							manager.DestroyEntity(ent);
					}
				}
			
		});
	}
};
