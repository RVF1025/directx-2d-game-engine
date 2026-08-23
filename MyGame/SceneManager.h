#pragma once
#include "Definitions.h"
#include "Scene.h"
#include "Stage1.h"
#include "StressStage.h"

class SceneManager : public Singleton<SceneManager>
{
public:
	enum Type
	{
		eStage1,
		eStress,
	};

public:
	SceneManager();
	~SceneManager();

public:

	Scene *m_Scene;

	void SceneChange(int scene);
};

