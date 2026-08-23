#include "SceneManager.h"


SceneManager::SceneManager()
{
	m_Scene = nullptr;
}


SceneManager::~SceneManager()
{
	delete m_Scene;
}

void SceneManager::SceneChange(int scene)
{
	if (m_Scene)
	{
		m_Scene->Release();
		delete m_Scene;
	}
	switch (scene)
	{
	case eStage1:
		m_Scene = new Stage1;
		break;
	case eStress:
		m_Scene = new StressStage;
		break;
	}
	if (m_Scene)
	{
		m_Scene->Init();
	}
}