#include <iostream>
#include <GL/glew.h>
#include <GL/glxew.h>
#include <GL/glxext.h>

#include "renderer.h"
#include "interface.h"
#include "camera.h"

SDL_Window* window;
SDL_GLContext glcontext;
int width;
int height;

bool interface::init(INIReader config){
	if(SDL_Init(SDL_INIT_VIDEO) < 0){
		std::cerr << "Failed to initialise SDL: " << SDL_GetError() << std::endl;
		return false;
	}

  width = config.GetInteger("interface", "width", 480);
  height = config.GetInteger("interface", "height", 320);

	window = SDL_CreateWindow("Protogee", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL);
	if(window == NULL){
		std::cerr << "Failed to create Window: %s" << SDL_GetError() << std::endl;
		return false;
	}
/*
	std::cout << glXQueryExtensionsString() << std::endl;
	unsigned int gpu_ids[2];
	glXGetGPUIDsAMD(1, gpu_ids);
	glcontext = glXCreateAssociatedContextAMD(gpu_ids[0], NULL);
	SDL_GL_MakeCurrent(window, glcontext);
*/	glcontext = SDL_GL_CreateContext(window);
	if(glcontext == NULL){
		std::cerr << "Failed to create GLContext: %s" << SDL_GetError() << std::endl;
		return false;
	}

	if(SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1) < 0){
		std::cerr << "Failed to accelerate GLContext: %s" << SDL_GetError() << std::endl;
		return false;
	}

  return true;
}

void interface::loop(){
  bool online = true;
  SDL_Event event;
  while(online){
    while(SDL_PollEvent(&event)){
      switch(event.type){
        case SDL_QUIT:
          online = false;
          break;
      }
	Camera::processSDLEvent(event);
	renderer::processSDLEvent(event);
    }
		Camera::update();
		renderer::update();
    SDL_GL_SwapWindow(window);
  }
}

SDL_GLContext interface::getGLContext(){
	return glcontext;
}

void interface::destroy(){
	SDL_GL_DeleteContext(glcontext);
	SDL_DestroyWindow(window);

	SDL_Quit();
}
