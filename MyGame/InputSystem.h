#pragma once
#include "MyECS.h"
#include "Components.h"
#include "Events.h"

class InputSystem : public System,
	public EventSubscriber<	ChangeStage	>
{
public:
	virtual void Init(Manager& manager) override
	{
		manager.SubscribeEvent<InputSystem, ChangeStage>();

		m_iStage = -1;
		m_bInput = true;
	}

	virtual void Tick(Manager& manager, float deltatime) override
	{
		if (InputManager::GetInstance()->IsKeyDown(DIK_0))
		{
			manager.AddEvent<ChangeStage>(0);
		}
		if (InputManager::GetInstance()->IsKeyDown(DIK_1))
		{
			manager.AddEvent<ChangeStage>(1);
		}
		if (InputManager::GetInstance()->IsKeyDown(DIK_2))
		{
			manager.AddEvent<ChangeStage>(2);
		}
		if (InputManager::GetInstance()->IsKeyDown(DIK_3))
		{
			manager.AddEvent<ChangeStage>(3);
		}
		if (InputManager::GetInstance()->IsKeyDown(DIK_4))
		{
			manager.AddEvent<ChangeStage>(4);
		}

		if (m_iStage == 0)
		{
			if (InputManager::GetInstance()->IsKeyDown(DIK_SPACE))
			{
				manager.AddEvent<ChangeStage>();
				//manager.AddEvent<InputEvent>("SPACE");
				m_bInput = true;
			}
		}
		else if (m_iStage == 4)
		{
			if (InputManager::GetInstance()->IsKeyDown(DIK_ESCAPE))
			{
				manager.AddEvent<ChangeStage>();
				//manager.AddEvent<InputEvent>("SPACE");
				m_bInput = true;
			}
		}
		else
		{
			if (InputManager::GetInstance()->IsKeyDown(DIK_ESCAPE))
			{
				manager.AddEvent<ChangeStage>(0);
			}
			if (m_bInput)
			{
				if (InputManager::GetInstance()->IsKeyDown(DIK_X))
				{
					manager.AddEvent<InputEvent>("X");
				}
				if (InputManager::GetInstance()->IsKeyDown(DIK_Z))
				{
					manager.AddEvent<InputEvent>("Z");
				}
			}
		}


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
	}
private:
	bool m_bInput;
	int m_iStage;
};
