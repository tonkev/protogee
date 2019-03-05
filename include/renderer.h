#ifndef RENDERER_H
#define RENDERER_H

#include "INIReader.h"
#define USE_OPENCL 1
#include "radeon_rays_cl.h"
#include <glm/glm.hpp>

namespace renderer{
  bool init(INIReader);
  RadeonRays::IntersectionApi* getIntersectionApi();
  glm::vec3 randomDirection(unsigned int);
  void update();
  void destroy();
}

#endif
