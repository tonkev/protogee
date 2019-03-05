#ifndef INTERFACE_H
#define INTERFACE_H

#include <SDL2/SDL.h>
#include "INIReader.h"

namespace interface{
  bool init(INIReader);
  void loop();
  SDL_GLContext getGLContext();
  void destroy();
}

#endif
