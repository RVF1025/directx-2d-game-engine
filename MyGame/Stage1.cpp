#include "Stage1.h"


#include <iostream>

Stage1::Stage1()
{
}

Stage1::~Stage1()
{
}

void Stage1::Init()
{
	Manager::GetInstance()->RegisterSystem<SoundSystem, InputSystem, StageSystem, PlayerUpdateSystem, AnimationSystem, CollisionSystem>();
	Manager::GetInstance()->RegisterRenderSystem<SpriteDrawSystem, FontDrawSystem>();

	Manager::GetInstance()->ExecuteEvent<ChangeStage>(0);
}

void Stage1::Update(float dt)
{
	Manager::GetInstance()->Tick(dt);
}

void Stage1::Render()
{
	Manager::GetInstance()->Render();
}

