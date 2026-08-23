// MyGame.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//
#include "Definitions.h"
#include "GameSystem.h"



INT WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, INT)
{
	GameSystem gameSystem;

	gameSystem.Init();
	gameSystem.Release();
}