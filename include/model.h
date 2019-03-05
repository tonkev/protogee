#ifndef MODEL_H
#define MODEL_H

#include "INIReader.h"
#include "radeon_rays.h"

namespace Model{
  bool init(INIReader, RadeonRays::IntersectionApi*);
  void draw(unsigned int);
  void destroy();
}

#endif
