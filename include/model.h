#ifndef MODEL_H
#define MODEL_H

#include "INIReader.h"
#include "radeon_rays.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Model{
  bool init(INIReader, RadeonRays::IntersectionApi*);
  void draw(unsigned int);
  void update(float);
  void destroy();
  glm::mat4 getModelMatrix();
  glm::vec4 getDiffuse(unsigned int, unsigned int, float, float);
  glm::vec4 getSpecular(unsigned int, unsigned int, float, float);
  glm::vec4 getNormal(unsigned int, unsigned int, float, float);
	std::string getTimeIntervals();
}

#endif
