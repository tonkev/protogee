#ifndef MODEL_H
#define MODEL_H

#include "INIReader.h"
#include "radeon_rays.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Model{
  bool init(INIReader, RadeonRays::IntersectionApi*);
  void draw(unsigned int);
  void destroy();
  glm::mat4 getModelMatrix();
  glm::vec3 getDiffuse(unsigned int, unsigned int, float, float);
  glm::vec3 getSpecular(unsigned int, unsigned int, float, float);
}

#endif
