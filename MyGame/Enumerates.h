#pragma once

enum class PlayerState
{
	IDLE,
	RUN,
	JUMP,
	SPIN,
	FALL,
	DEATH,
};

enum class GravityDirection
{
	UP,
	DOWN,
	LEFT,
	RIGHT,
};