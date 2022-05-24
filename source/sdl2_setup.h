#ifndef SDL2_SETUP_H
#define SDL2_SETUP_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <GL/glu.h>

#define SDL_SETUP_MAX_ERROR_STRING_BUFFER_LENGTH 512

typedef enum
{
  SDL2_SETUP_SUCCESS,
  SDL2_SETUP_ERROR
} ESdl2SetupResultType;

typedef struct
{
  ESdl2SetupResultType setup_result;
  char error_message[SDL_SETUP_MAX_ERROR_STRING_BUFFER_LENGTH];
  SDL_Window * p_window;
} SSDL2SetupResult;

char * sdl2_setup_error_string(void);
SSDL2SetupResult sdl2_setup_for_2d_rendering(int screen_width, int screen_height, char * screen_title);
void sdl2_setup_teardown(SSDL2SetupResult setup_result);

#endif
