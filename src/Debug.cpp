#include "Debug.h"
#include <iostream>

/*static*/ void Debug::PrintKeyInfo(SDL_KeyboardEvent* key)
{
	/* Is it a release or a press? */
	if (key->type == SDL_KEYUP)
		printf("Release:- ");
	else
		printf("Press:- ");

	/* Print the hardware scancode first */
	// scan code is generally hardware specific
	printf("Scancode: 0x%02X", key->keysym.scancode);
	/* Print the name of the key */
	// the virtual key interpretation of the scancode, you should use this
	printf(", Name: %s", SDL_GetKeyName(key->keysym.sym));

	printf("\n");
	/* Print modifier info */
	PrintModifiers((SDL_Keymod)key->keysym.mod);
}


/* Print modifier info */
/*static*/ void Debug::PrintModifiers(SDL_Keymod mod)
{
	printf("Modifers: ");

	/* If there are none then say so and return */
	if (mod == KMOD_NONE) {
		printf("None\n");
		return;
	}

	/* Check for the presence of each SDLMod value */
	/* This looks messy, but there really isn't    */
	/* a clearer way.                              */
	if (mod & KMOD_NUM) printf("NUMLOCK ");
	if (mod & KMOD_CAPS) printf("CAPSLOCK ");
	if (mod & KMOD_LCTRL) printf("LCTRL ");
	if (mod & KMOD_RCTRL) printf("RCTRL ");
	if (mod & KMOD_RSHIFT) printf("RSHIFT ");
	if (mod & KMOD_LSHIFT) printf("LSHIFT ");
	if (mod & KMOD_RALT) printf("RALT ");
	if (mod & KMOD_LALT) printf("LALT ");
	if (mod & KMOD_CTRL) printf("CTRL ");
	if (mod & KMOD_SHIFT) printf("SHIFT ");
	if (mod & KMOD_ALT) printf("ALT ");
	printf("\n");
}
