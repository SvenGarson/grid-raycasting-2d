#include "sdl2_setup.h"
#include <stdio.h>
#include <string.h>

static char * ERROR_MESSAGE_PREFIX = "[SDL2 Setup] ";

static void set_error_message
(
  SSDL2SetupResult * result,
  const char * error_message,
  const char * additional_error_message
)
{
  // If some optional error message extentsion is given, append it in the
  // format '- extension'. Otherwise append nothing
  int append_optional_error_message
    = additional_error_message && (strlen(additional_error_message) > 0);

  if (append_optional_error_message)
  {
    snprintf(
      result->error_message,
      SDL_SETUP_MAX_ERROR_STRING_BUFFER_LENGTH,
      "%s%s - %s", ERROR_MESSAGE_PREFIX, error_message, additional_error_message
    );
  }
  else
  {
    snprintf(
      result->error_message,
      SDL_SETUP_MAX_ERROR_STRING_BUFFER_LENGTH,
      "%s%s", ERROR_MESSAGE_PREFIX, error_message
    );
  }
}

static SSDL2SetupResult make_setup_result
(
  ESdl2SetupResultType result_type,
  SDL_Window * p_window,
  const char * error_message,
  const char * additional_error_message
)
{
  SSDL2SetupResult result;

  result.setup_result = result_type;
  result.p_window = p_window;

  // When error occured:
  if (result_type == SDL2_SETUP_ERROR)
  {
    // A - Free the window resource now and pass NULL as result to the caller
    SDL_DestroyWindow(result.p_window);
    result.p_window = NULL;

    // B - Set the error message
    set_error_message(&result, error_message, additional_error_message);
  }

  return result;
}

SSDL2SetupResult sdl2_setup_for_2d_rendering
(
  int screen_width,
  int screen_height,
  char * screen_title
)
{
  // Attempt to initialize the SDL2 video sub-systems
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
  {
    // SDL2 could not be initialized
    return make_setup_result
    (
      SDL2_SETUP_ERROR, NULL, "SDL2 video could not be initialized", SDL_GetError()
    );
  }

  // Video sub-system initialized
  // Attempt to create the window within a specific OpenGL version
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

  // Setup the window to render into
  SDL_Window * p_window = SDL_CreateWindow(
    screen_title,
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    screen_width, screen_height,
    SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
  );

  if (p_window == NULL)
  {
    // Window could not be created
    return make_setup_result(
      SDL2_SETUP_ERROR, NULL, "SDL2 window could not be created", SDL_GetError()
    );
  }

  // Window created - Now setup OpenGL context for the created window
  SDL_GLContext openglContext = SDL_GL_CreateContext(p_window);
  if (openglContext == NULL)
  {
    return make_setup_result(
      SDL2_SETUP_ERROR, p_window, "OpenGL context could not be created", SDL_GetError()
    );
  }

  // OpenGL context created - Now initialize OpenGL state
  // Enable VSync
  if (SDL_GL_SetSwapInterval(1) < 0)
  {
    return make_setup_result(
      SDL2_SETUP_ERROR, p_window, "Could not enable VSync", SDL_GetError()
    );
  }

  // Set OpenGL matrices and color for 2D rendering
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, screen_width, 0, screen_height);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClearColor(0.0f, 0.0f, 1.0f, 1.0f);

  // Check for errors on initial OpenGL state changes
  GLenum gl_error = glGetError();
  if (gl_error != GL_NO_ERROR)
  {
    return make_setup_result(
      SDL2_SETUP_ERROR,
      p_window,
      "Could not set OpenGL state (matrices; clear color; ...)",
      (const char *) gluErrorString(gl_error)
    );
  }

  // Success
  return make_setup_result(SDL2_SETUP_SUCCESS, p_window, NULL, NULL);
}

void sdl2_setup_teardown(SSDL2SetupResult setup_result)
{
  // Destroy the window and quit all initialized SDL2 sub-systems
  SDL_DestroyWindow(setup_result.p_window);
  SDL_Quit();
}
