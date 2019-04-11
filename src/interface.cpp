#include <iostream>
#include <GL/glew.h>
#include <GL/glxew.h>
#include <GL/glxext.h>

#include "renderer.h"
#include "interface.h"
#include "camera.h"
#include "model.h"

SDL_Window* window;
SDL_GLContext glcontext;
int width;
int height;
float deltaTime;
float lastFrame;
float maxTime = 0;
bool interactive;

bool interface::init(INIReader config){
	if(SDL_Init(SDL_INIT_VIDEO) < 0){
		std::cerr << "Failed to initialise SDL: " << SDL_GetError() << std::endl;
		return false;
	}

  width = config.GetInteger("interface", "width", 480);
  height = config.GetInteger("interface", "height", 320);
  maxTime = config.GetReal("interface", "time", 0);
	interactive = config.GetBoolean("interface", "interactive", true);

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
*/	
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	//SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	glcontext = SDL_GL_CreateContext(window);
	if(glcontext == NULL){
		std::cerr << "Failed to create GLContext: %s" << SDL_GetError() << std::endl;
		return false;
	}

	if(SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1) < 0){
		std::cerr << "Failed to accelerate GLContext: %s" << SDL_GetError() << std::endl;
		return false;
	}
	
  deltaTime = 0.0f;
  lastFrame = 0.0f;

  return true;
}

void interface::loop(){
  bool online = true;
  SDL_Event event;
  while(online){
		if(interactive){
		  while(SDL_PollEvent(&event)){
		    switch(event.type){
		      case SDL_QUIT:
		        online = false;
		        break;
		    }
				Camera::processSDLEvent(event);
				renderer::processSDLEvent(event);
		  }
		}
		float currentFrame = SDL_GetTicks();
		deltaTime = (currentFrame - lastFrame) / 1000.0f;
		lastFrame = currentFrame;
		Camera::update(deltaTime);
		Model::update(deltaTime);
		renderer::update(deltaTime);
    SDL_GL_SwapWindow(window);
		
		if(maxTime != 0 && currentFrame > maxTime){/*
			unsigned int *screenPixels = new unsigned int[width * height];
			glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_INT, screenPixels);
			SDL_Surface *bitmap = SDL_CreateRGBSurfaceFrom(screenPixels, width, height, 32, width*4, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
			SDL_SaveBMP(bitmap, "screenshot.bmp");
			SDL_FreeSurface(bitmap);*/
			online = false;
			break;
		}
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
