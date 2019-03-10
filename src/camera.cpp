#include <iostream>
#include "camera.h"

glm::vec3 position;
glm::vec3 front;
glm::vec3 up;
float yaw;
float pitch;
float speed;
bool w, a, s ,d, c;
bool mouseRel;

bool Camera::init(INIReader config){

  speed = config.GetReal("camera", "speed", 2.5f);

  position = glm::vec3(0.0f, 0.0f, 3.0f);
  position.y = config.GetReal("camera", "posY", 0.f);
  position.z = config.GetReal("camera", "posZ", 0.f);
  front = glm::vec3(0.0f, 0.0f, 1.0f);
  up = glm::vec3(0.0f, 1.0f, 0.0f);
  yaw = config.GetReal("camera", "yaw", -90.f);
  pitch = 0.0f;


w = false;
a = false;
s = false;
d = false;
c = false;

mouseRel = false;
	
  front.x = cos(glm::radians(pitch)) * cos(glm::radians(yaw));
  front.y = sin(glm::radians(pitch));
  front.z = cos(glm::radians(pitch)) * sin(glm::radians(yaw));
  front = glm::normalize(front);

  return true;
}

void Camera::processSDLEvent(SDL_Event event){
  switch(event.type){
    case SDL_KEYDOWN:
      switch(event.key.keysym.sym){
        case SDLK_w:
          w = true;
          break;
        case SDLK_a:
          a = true;
          break;
        case SDLK_s:
          s = true;
          break;
        case SDLK_d:
          d = true;
          break;
        case SDLK_c:
          c = true;
          break;
      }
      break;
    case SDL_KEYUP:
      switch(event.key.keysym.sym){
        case SDLK_w:
          w = false;
          break;
        case SDLK_a:
          a = false;
          break;
        case SDLK_s:
          s = false;
          break;
        case SDLK_d:
          d = false;
          break;
        case SDLK_c:
          c = false;
          break;
      }
      break;
    case SDL_MOUSEMOTION:
      float sensitivity = 0.1f;
      yaw += event.motion.xrel * sensitivity;
      pitch -= event.motion.yrel * sensitivity;
      if(pitch > 89.0f) pitch = 89.0f;
      if(pitch < -89.0f) pitch = -89.0f;
      front.x = cos(glm::radians(pitch)) * cos(glm::radians(yaw));
      front.y = sin(glm::radians(pitch));
      front.z = cos(glm::radians(pitch)) * sin(glm::radians(yaw));
      front = glm::normalize(front);
      break;
  }
}

void Camera::update(float deltaTime){

    //std::cout << 1/deltaTime << std::endl;

		float step = speed * deltaTime;
		if(w) position += step * front;
		if(s) position -= step * front;
		if(a) position -= glm::normalize(glm::cross(front, up)) * step;
		if(d) position += glm::normalize(glm::cross(front, up)) * step;
		if(c){
			if(mouseRel){
				SDL_SetRelativeMouseMode(SDL_FALSE);
				mouseRel = false;
			}else{
				SDL_SetRelativeMouseMode(SDL_TRUE);
				mouseRel = true;
			}
			c = false;
		}
}

glm::mat4 Camera::getViewMatrix(){
  return glm::lookAt(position, position + front, up);
}

glm::vec3 Camera::getPosition(){
  return position;
}
