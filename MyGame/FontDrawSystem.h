#pragma once
#include "MyECS.h"
#include "Components.h"
#include "Events.h"
#include "GraphicManager.h"

#include <string>

class FontDrawSystem : public System, 
	public EventSubscriber<PlayerRespawn>,
	public EventSubscriber<ChangeStage>

{
public:
	virtual void Init(Manager& manager) override
	{
		manager.SubscribeEvent<FontDrawSystem, PlayerRespawn>();
		manager.SubscribeEvent<FontDrawSystem, ChangeStage>();
		m_iDeathCount = 0;
		m_iDeathSum = 0;
		m_iStage = 0;

		m_Renderer = GraphicManager::GetInstance()->GetRenderer();
	}

	virtual void Tick(Manager& manager, float deltatime) override
	{
		m_Renderer->FontRenderStart();

		auto pLargeFont = TextureManager::GetInstance()->GetLargeFont();
		auto pSmallFont = TextureManager::GetInstance()->GetSmallFont();

		if (m_iStage == 0)
		{
			m_iDeathCount = 0;
			m_iDeathSum = 0;

			m_Renderer->DrawText(pLargeFont, "Bit Bit 8 Bit Jump!", 180, 210, { 1.f, 1.f, 1.f, 1.f });
			m_Renderer->DrawText(pSmallFont, "Press Space Bar to Play", 250, 610, { 1.f, 1.f, 1.f, 1.f });
		}
		else if (m_iStage != 0 && m_iStage != 4)
		{
			m_Renderer->DrawText(pSmallFont, ("Stage " + std::to_string(m_iStage)), 10, 10, { 1.f, 0.2f, 0.6f, 1.f });

			if (m_iDeathCount == 0)
				m_Renderer->DrawText(pSmallFont, "Perfect Play!", 10, 50, { 0.8f, 0.8f, 0.2f, 1.f });
			else if (m_iDeathCount == 1)
				m_Renderer->DrawText(pSmallFont, ("You Died " + std::to_string(m_iDeathCount) + " Time."), 10, 50, { 1.f, 0.2f, 0.2f, 1.f });
			else
				m_Renderer->DrawText(pSmallFont, ("You Died " + std::to_string(m_iDeathCount) + " Times."), 10, 50, { 1.f, 0.2f, 0.2f, 1.f });
		}
		else
		{
			m_Renderer->DrawText(pLargeFont, "Game Clear!", 250, 210, { 1.f, 1.f, 1.f, 1.f });

			if (m_iDeathSum == 0)
			{
				m_Renderer->DrawText(pSmallFont, "Perfect Play!", 300, 550, { 0.8f, 0.8f, 0.2f, 1.f });
			}
			else if (m_iDeathSum == 1)
			{
				m_Renderer->DrawText(pSmallFont, ("You Died " + std::to_string(m_iDeathSum) + " Times."), 250, 550, { 1.f, 0.2f, 0.2f, 1.f });
			}
			else
			{
				m_Renderer->DrawText(pSmallFont, ("You Died " + std::to_string(m_iDeathSum) + " Times."), 250, 550, { 1.f, 0.2f, 0.2f, 1.f });
			}

			m_Renderer->DrawText(pSmallFont, "Press ESC to Title", 250, 610, { 1.f, 1.f, 1.f, 1.f });
		}

		m_Renderer->FontRenderEnd();
	}

	virtual void Received(Manager& manager, PlayerRespawn* event) override
	{
		m_iDeathCount++;
	}

	virtual void Received(Manager& manager, ChangeStage* event) override
	{
		if (event->m_iStage != -1)
			m_iStage = event->m_iStage;
		else
		{
			if (m_iStage == 4)
				m_iStage = 0;
			else
			m_iStage++;
		}
		m_iDeathSum += m_iDeathCount;
		m_iDeathCount = 0;
	}


private:
	int m_iDeathCount;
	int m_iDeathSum;
	int m_iStage;
	RECT m_Rect;
	IRenderer* m_Renderer;
};
