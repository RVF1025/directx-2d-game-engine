#pragma once
#include "MyECS.h"
#include "Components.h"
#include "Events.h"
#include "GraphicManager.h"
#include "IRenderer.h"

class SpriteDrawSystem : public System
{
public:
	struct Rect
	{
		float left;
		float top;
		float right;
		float bottom;
	};

	inline bool Intersects(const Rect& a, const Rect& b)
	{
		return !(a.right  < b.left ||
			a.left   > b.right ||
			a.bottom < b.top ||
			a.top    > b.bottom);
	}

	virtual void Init(Manager& manager) override
	{
		m_Player = nullptr;
		m_Renderer = GraphicManager::GetInstance()->GetRenderer();
	}

	virtual void Tick(Manager& manager, float deltatime) override
	{
		m_Renderer->SpriteRenderStart();

		float cameraX = 0.f;
		float cameraY = 0.f;

		if (m_Player != nullptr)
		{
			auto p = m_Player->GetComponent<Position>();
			cameraX = p->m_fPosX - SCREEN_WIDTH * 0.5f;
			cameraY = p->m_fPosY - SCREEN_HEIGHT * 0.5f;
		}

		Rect camera;
		camera.left = cameraX;
		camera.top = cameraY;
		camera.right = cameraX + SCREEN_WIDTH;
		camera.bottom = cameraY + SCREEN_HEIGHT;

		for (auto ent : m_Entities)
		{
			auto sp = ent->GetComponent<Sprite>();
			auto pos = ent->GetComponent<Position>();
			auto ani = ent->GetComponent<Animation>();
			auto rot = ent->GetComponent<Rotation>();
			
			if (!sp || !pos)
				continue;

			if (!sp->m_bIsActive)
				continue;

			TextureHandle* handle;

			if (ani != nullptr)
			{
				handle = TextureManager::GetInstance()->GetTexture(ani->m_SpriteState, (int)ani->m_fCurIndex);
			}
			else
			{
				handle = TextureManager::GetInstance()->GetTexture(sp->m_FileName);
			}

			if (handle == nullptr)
				continue;

			float w = handle->width * sp->m_fScaleX;
			float h = handle->height * sp->m_fScaleY;

			Rect sprite;
			sprite.left = pos->m_fPosX - w * 0.5f;
			sprite.top = pos->m_fPosY - h * 0.5f;
			sprite.right = pos->m_fPosX + w * 0.5f;
			sprite.bottom = pos->m_fPosY + h * 0.5f;

			if (!Intersects(camera, sprite))
				continue;

			m_Renderer->DrawSprite(
				handle,
				pos->m_fPosX - cameraX,
				pos->m_fPosY - cameraY,
				sp->m_fScaleX,
				sp->m_fScaleY,
				rot ? rot->m_fRotZ : 0.f,
				sp->m_bFlip
				);
		}

		m_Renderer->SpriteRenderEnd();
	}

	virtual void Received(Manager& manager, OnEntityCreated* event) override
	{
		if (event->m_Entity->IsHavingComponent<Position, Sprite>())
		{
			m_Entities.push_back(event->m_Entity);
		}
		if (event->m_Entity->IsHavingComponent<Player>())
		{
			m_Player = event->m_Entity;
		}
	}

	virtual void Received(Manager& manager, OnEntityDestroyed* event) override
	{
		if (event->m_Entity->IsHavingComponent<Position, Sprite>())
		{
			m_Entities.erase(std::remove(m_Entities.begin(), m_Entities.end(), event->m_Entity));
		}
		if (event->m_Entity->IsHavingComponent<Player>())
		{
			m_Player = nullptr;
		}
	}

private:
	std::vector<Entity*> m_Entities;
	Entity* m_Player;
	IRenderer* m_Renderer;
};
