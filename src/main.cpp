#include <iostream>
#include <unistd.h>
#include "INIReader.h"
#include "interface.h"
#include "renderer.h"
#include "model.h"
#include "camera.h"

int main(int argc, char** args){
  if(argc == 2)
	chdir(args[1]);
  
  INIReader config("config.ini");
  if(config.ParseError()){
    std::cerr << "Failed to parse 'config.ini'" << std::endl;
    return EXIT_FAILURE;
  }

  if(!interface::init(config)){
    std::cerr << "Failed to initialise interface" << std::endl;
    return EXIT_FAILURE;
  }

  if(!Camera::init(config)){
    std::cerr << "Failed to initialise camera" << std::endl;
    return EXIT_FAILURE;
  }

  if(!renderer::init(config)){
    std::cerr << "Failed to initialise renderer" << std::endl;
  }else if(!Model::init(config, renderer::getIntersectionApi())){
    std::cerr << "Failed to initialise model" << std::endl;
  }else {
    interface::loop();
  }

  Model::destroy();
  renderer::destroy();
  interface::destroy();
  return EXIT_SUCCESS;
}
