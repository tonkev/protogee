#ifndef CAMERA_H
#define CAMERA_H

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "INIReader.h"

namespace Camera{
  bool init(INIReader);
  void processSDLEvent(SDL_Event event);
  void update();
  glm::mat4 getViewMatrix();
  glm::vec3 getPosition();
};

#endif
