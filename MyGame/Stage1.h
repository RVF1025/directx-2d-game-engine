#pragma once
#include "Scene.h"
#include "MyECS.h"
#include "Systems.h"

class Stage1 : public Scene
{
public:
	Stage1();
	~Stage1();

	virtual void Init();
	virtual void Update(float dt);
	virtual void Render();

};
