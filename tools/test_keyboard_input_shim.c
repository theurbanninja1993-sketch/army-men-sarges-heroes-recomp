/* Linux-only input shim for armymensh_controls_tests. Never loaded by the game normally. */
#define _GNU_SOURCE
#include <SDL3/SDL.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
const bool* SDL_GetKeyboardState(int* n) {
 static const bool* (*original)(int*);static bool keys[SDL_SCANCODE_COUNT];
 if(!original)original=dlsym(RTLD_NEXT,"SDL_GetKeyboardState");
 int count=0;const bool* real=original(&count);memcpy(keys,real,sizeof keys);if(n)*n=count;
 const char* s=getenv("ARMY_INPUT_KEYS");if(!s)return keys;
 keys[SDL_SCANCODE_W]=strstr(s,"w")!=0; keys[SDL_SCANCODE_S]=strstr(s,"s,")!=0;
 keys[SDL_SCANCODE_A]=strstr(s,"a")!=0;keys[SDL_SCANCODE_D]=strstr(s,"d")!=0;
 keys[SDL_SCANCODE_LSHIFT]=strstr(s,"shift")!=0;
 return keys;
}
SDL_MouseButtonFlags SDL_GetMouseState(float* x,float* y) {
 static SDL_MouseButtonFlags (*original)(float*,float*);
 if(!original)original=dlsym(RTLD_NEXT,"SDL_GetMouseState");
 SDL_MouseButtonFlags flags=original(x,y);const char* s=getenv("ARMY_INPUT_KEYS");
 if(s && strstr(s,"r"))flags|=SDL_BUTTON_MASK(SDL_BUTTON_RIGHT);
 return flags;
}
