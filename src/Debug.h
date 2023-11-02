#pragma once
#include <SDL.h>

// TODO: Figure out if there is a nice way to forward delcare the SDL_Keymod enum

class Debug
{
public:
	static void PrintKeyInfo(SDL_KeyboardEvent* key);
	static void PrintModifiers(SDL_Keymod mod); 
private:
	Debug() = default;
	~Debug() = default;
};