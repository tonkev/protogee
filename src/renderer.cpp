#include <iostream>
#include <fstream>
#include <GL/glew.h>
#include <GL/glxew.h>
#include <stdlib.h>
#include <cmath>
#include <CL/cl.h>
#include <CL/cl_gl.h>

#include "model.h"
#include "renderer.h"
#include "camera.h"
#include "interface.h"
#include "halton.hpp"

#define LOG_MESSAGE_LENGTH 512

namespace RR = RadeonRays;

RR::IntersectionApi* intersectionApi;
glm::mat4 projection;
glm::mat4 invPrevView;
glm::mat4 invPrevProjection;

struct Light{
  glm::vec4 position;
  glm::vec4 diffuse;
  glm::vec4 specular;
  glm::vec4 normal;
};

Light pl;
std::vector<Light> vpls;
std::vector<bool> validVPLs;

unsigned int p_width;
unsigned int p_height;

unsigned int dpth_fbo;
unsigned int dpth_width;
unsigned int dpth_height;
unsigned int dpth_cbmp;
glm::mat4 dpth_prjctn;
std::vector<glm::mat4> dpth_trnsfrms;
float dpth_far_plane;
unsigned int dpth_shader;

unsigned int vpl_vao;
unsigned int vpl_vbo;
unsigned int vpl_count;
unsigned int vpl_shader;

unsigned int gBuffer, gPosition, gNormal, gAlbedo, gSpecular, gBufferShader;
unsigned int dPlaneVAO, dPlaneVBO, dPlaneShader;

unsigned int noOfVPLS;
unsigned int maxVPLGenPerFrame;
unsigned int vMasks;
unsigned int iPlaneShader;

cl_context clContext;
cl_command_queue clQueue;
cl_mem clPositions;
cl_mem clNormals;
cl_mem clSpeculars;
cl_mem clRays;
cl_mem clIsects;
cl_mem clOcclus;
cl_mem clVPLs;
cl_mem clMasks;
cl_program clProgram;
cl_kernel clInitMasksKernel;
cl_kernel clPreRaysKernel;
cl_kernel clPostRaysKernel;

RR::Buffer* rrRays;
RR::Buffer* rrIsects;
RR::Buffer* rrOcclus;

#define MAX_NO_OF_VPLS 512

RR::ray vplRays[MAX_NO_OF_VPLS];
std::vector<RR::Intersection> vplIsects;
std::vector<int> vplOcclus;
RR::Buffer* vplRayBuffer;
RR::Buffer* vplIsectBuffer;
RR::Buffer* vplOccluBuffer;

unsigned int interleavedSamplingSize;
unsigned int vplNo;

float rDelta;

unsigned int dBuffer, iBuffer, dColor, iColor;
unsigned int discShader, discBuffer1, discBuffer2, discIndirect1, discIndirect2;

float lightSpeed;
bool i = false;
bool j = false;
bool k = false;
bool l = false;
bool u = false;
bool o = false;
int debugVPL = -1;
bool directEnabled = true;
bool indirectEnabled = true;
bool vplDebugEnabled = false;
bool vplUpdated = true;

std::vector<glm::mat4> viewHistory;
unsigned int iHistory, iHistorySize, iHistoryShader, pHistory;
int iHistoryIndex;

float lightRadius;
float areaLightChance;

int currVPL;
int pastVPL = -1;
int noOfInvalidVPLs;
int noOfVPLBounces;

void renderer::processSDLEvent(SDL_Event event){
  switch(event.type){
	  case SDL_KEYDOWN:
	    switch(event.key.keysym.sym){
	      case SDLK_1:
	        directEnabled = !directEnabled;
	        break;
	      case SDLK_2:
	        indirectEnabled = !indirectEnabled;
	        break;
	      case SDLK_3:
	        vplDebugEnabled = !vplDebugEnabled;
	        break;
	      case SDLK_i:
	        i = true;
	        break;
	      case SDLK_j:
	        j = true;
	        break;
	      case SDLK_k:
	        k = true;
	        break;
	      case SDLK_l:
	        l = true;
	        break;
	      case SDLK_u:
	        u = true;
	        break;
	      case SDLK_o:
	        o = true;
	        break;
	      case SDLK_r:
	        debugVPL = ((debugVPL + 2) % (noOfVPLS + 1)) - 1;
	        break;
	      case SDLK_f:
	        debugVPL = ((debugVPL) % (noOfVPLS + 1)) - 1;
	        break;
	    }
	    break;
	 case SDL_KEYUP:
       switch(event.key.keysym.sym){
		  case SDLK_i:
 	        i = false;
 	        break;
 	      case SDLK_j:
 	        j = false;
 	        break;
 	      case SDLK_k:
 	        k = false;
 	        break;
 	      case SDLK_l:
 	        l = false;
 	        break;
 	      case SDLK_u:
 	        u = false;
 	        break;
 	      case SDLK_o:
 	        o = false;
 	        break;
       }
       break;
  }
}

bool loadShader(const char* path, unsigned int shader){
  std::ifstream file;
  file.open(path);
	if(!file.is_open()){
    std::cerr << "Failed to open shader: " << path << std::endl;
    return false;
  }
  std::filebuf* rdbuf = file.rdbuf();
  std::size_t size = rdbuf->pubseekoff(0, file.end, file.in);
  rdbuf->pubseekpos(0, file.in);
  char* buffer = new char[size+1];
  rdbuf->sgetn(buffer, size);
	buffer[size] = '\0';
  file.close();

	GLchar const* source[] = {buffer};
	glShaderSource(shader, 1, source, NULL);
	glCompileShader(shader);
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if(!success){
		GLchar infoLog[LOG_MESSAGE_LENGTH];
		glGetShaderInfoLog(shader, LOG_MESSAGE_LENGTH, NULL, infoLog);
		std::cerr << "Failed to compile shader " << path << ":" << std::endl << infoLog << std::endl;
		return false;
	}
	return true;
}

unsigned int initShader(const char* vertexPath, const char* fragmentPath, const char* geometryPath="\0"){
  unsigned int vertex, fragment, geometry;
  vertex = glCreateShader(GL_VERTEX_SHADER);
  fragment = glCreateShader(GL_FRAGMENT_SHADER);
  geometry = glCreateShader(GL_GEOMETRY_SHADER);
  if(!loadShader(vertexPath, vertex)){
		std::cerr << "Failed to load vertex shader" << std::endl;
    return 0;
	}
  if(!loadShader(fragmentPath, fragment)){
		std::cerr << "Failed to load fragment shader" << std::endl;
    return 0;
	}
  if(geometryPath[0] != '\0'){
    if(!loadShader(geometryPath, geometry)){
  		std::cerr << "Failed to load geometry shader" << std::endl;
      return 0;
  	}
  }

  unsigned int id = glCreateProgram();
  if(id == 0){
    std::cerr << "Failed to create shader program" << std::endl;
    return 0;
  }

  glAttachShader(id, vertex);
  glAttachShader(id, fragment);
  if(geometryPath[0] != '\0')
    glAttachShader(id, geometry);
  glLinkProgram(id);

  GLint success;
  glGetProgramiv(id, GL_LINK_STATUS, &success);
  if(!success){
    GLchar infoLog[LOG_MESSAGE_LENGTH];
		glGetShaderInfoLog(id, LOG_MESSAGE_LENGTH, NULL, infoLog);
		std::cerr << "Failed to link shader program :" << std::endl << infoLog << std::endl;
  }

  glDeleteShader(vertex);
  glDeleteShader(fragment);
  glDeleteShader(geometry);

  return id;
}

bool renderer::init(INIReader config){
  GLenum glewError = glewInit();
  if(glewError != GLEW_OK){
    std::cerr << "Failed to initialise GLEW: " << glewGetErrorString(glewError) << std::endl;
    return false;
  }

  cl_platform_id platforms[1];
  cl_device_id devices[1];
  cl_int clErr;
  clGetPlatformIDs(1, platforms, NULL);
  clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 1, devices, NULL);
  cl_context_properties props[] = {
	CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[0],
	CL_GL_CONTEXT_KHR, (cl_context_properties)glXGetCurrentContext(),
	CL_GLX_DISPLAY_KHR, (cl_context_properties)glXGetCurrentDisplay(),
    0
  };
  //clGetGLContextInfoKHR(props, CL_DEVICES_FOR_GL_CONTEXT_KHR, 0, devices, NULL);

  clContext = clCreateContext(props, 1, devices, NULL, NULL, &clErr);
  //std::cout << clErr << std::endl;
  clQueue = clCreateCommandQueueWithProperties(clContext, devices[0], NULL, NULL);
  intersectionApi = RR::CreateFromOpenClContext(clContext, devices[0], clQueue);
  //intersectionApi->SetOption("bvh.builder", "sah");

  p_width = config.GetInteger("interface", "width", 480);
  p_height = config.GetInteger("interface", "height", 320);

	//model = glm::mat4(1.0f);
  //model = glm::scale(model, glm::vec3(0.1f));
  projection = glm::perspective(glm::radians(45.0f), p_width / float(p_height), 0.1f, 10000.0f);
  invPrevProjection = projection;

  pl.position = glm::vec4(0, 10, 0, 1);
  pl.position.y = config.GetReal("renderer", "LightY", 1);
  float lightR = config.GetReal("renderer", "LightR", 1);
  float lightG = config.GetReal("renderer", "LightG", 1);
  float lightB = config.GetReal("renderer", "LightB", 1);
  pl.diffuse = glm::vec4(glm::vec3(lightR, lightG, lightB), 1);
  pl.specular = glm::vec4(glm::vec3(lightR, lightG, lightB), 1);

  glGenFramebuffers(1, &dpth_fbo);
  glGenTextures(1, &dpth_cbmp);
  glBindTexture(GL_TEXTURE_CUBE_MAP, dpth_cbmp);
  dpth_width = config.GetInteger("renderer", "shadow_map_size", 1024);
  dpth_height = config.GetInteger("renderer", "shadow_map_size", 1024);
  dpth_far_plane = config.GetReal("renderer", "depth_far_plane", 10);
  for(unsigned int i = 0; i < 6; ++i)
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, dpth_width, dpth_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  dpth_prjctn = glm::perspective(glm::radians(90.0f), dpth_width/(float)dpth_height, 1.0f, dpth_far_plane);
  glm::vec3 plpos = glm::vec3(pl.position);
  dpth_trnsfrms.push_back(dpth_prjctn * glm::lookAt(plpos, plpos + glm::vec3( 1, 0, 0), glm::vec3( 0,-1, 0)));
  dpth_trnsfrms.push_back(dpth_prjctn * glm::lookAt(plpos, plpos + glm::vec3(-1, 0, 0), glm::vec3( 0,-1, 0)));
  dpth_trnsfrms.push_back(dpth_prjctn * glm::lookAt(plpos, plpos + glm::vec3( 0, 1, 0), glm::vec3( 0, 0, 1)));
  dpth_trnsfrms.push_back(dpth_prjctn * glm::lookAt(plpos, plpos + glm::vec3( 0,-1, 0), glm::vec3( 0, 0,-1)));
  dpth_trnsfrms.push_back(dpth_prjctn * glm::lookAt(plpos, plpos + glm::vec3( 0, 0, 1), glm::vec3( 0,-1, 0)));
  dpth_trnsfrms.push_back(dpth_prjctn * glm::lookAt(plpos, plpos + glm::vec3( 0, 0,-1), glm::vec3( 0,-1, 0)));

  glBindFramebuffer(GL_FRAMEBUFFER, dpth_fbo);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, dpth_cbmp, 0);
	glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  dpth_shader = initShader("src/shaders/dpth.vsh", "src/shaders/dpth.fsh", "src/shaders/dpth.gsh");
  if(dpth_shader == 0){
    std::cerr << "Failed to initialise dpth shader" << std::endl;
    return false;
  }

  glGenFramebuffers(1, &gBuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
  glGenTextures(1, &gPosition);
  glBindTexture(GL_TEXTURE_2D, gPosition); //MUST BE RGBA FOR OPENCL INTEROP
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, p_width, p_height, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

  glGenTextures(1, &gNormal);
  glBindTexture(GL_TEXTURE_2D, gNormal);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, p_width, p_height, 0, GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

  glGenTextures(1, &gAlbedo);
  glBindTexture(GL_TEXTURE_2D, gAlbedo);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, p_width, p_height, 0, GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedo, 0);

  glGenTextures(1, &gSpecular);
  glBindTexture(GL_TEXTURE_2D, gSpecular);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, p_width, p_height, 0, GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, gSpecular, 0);

  unsigned int attachments[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
  glDrawBuffers(4, attachments);

  unsigned int rboDepth;
	glGenRenderbuffers(1, &rboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, p_width, p_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
		std::cerr << "Framebuffer not complete." << std::endl;
	  glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return false;
  }
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  vpl_shader = initShader("src/shaders/vpl.vsh", "src/shaders/vpl.fsh");
  if(vpl_shader == 0){
    std::cerr << "Failed to initialise VPL shader" << std::endl;
    return false;
  }

  gBufferShader = initShader("src/shaders/gbuffer.vsh", "src/shaders/gbuffer.fsh");
  if(gBufferShader == 0){
    std::cerr << "Failed to initialise G-Buffer shader" << std::endl;
    return false;
  }

  float vertices[] = {
    -1.0f, -1.0f, 0.0f, 0.0f,
     1.0f, -1.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 1.0f,
     1.0f,  1.0f, 1.0f, 1.0f
  };
  glGenBuffers(1, &dPlaneVBO);
  glGenVertexArrays(1, &dPlaneVAO);
  glBindVertexArray(dPlaneVAO);
  glBindBuffer(GL_ARRAY_BUFFER, dPlaneVBO);
  glBufferData(GL_ARRAY_BUFFER, 24 * sizeof(GL_FLOAT), vertices, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GL_FLOAT), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GL_FLOAT), (void*)(2 * sizeof(GL_FLOAT)));

  dPlaneShader = initShader("src/shaders/dplane.vsh", "src/shaders/dplane.fsh");
  if(dPlaneShader == 0){
    std::cerr << "Failed to initialise D-Plane shader" << std::endl;
    return false;
  }

  iPlaneShader = initShader("src/shaders/iplane.vsh", "src/shaders/iplane.fsh");
  if(iPlaneShader == 0){
    std::cerr << "Failed to initialise I-Plane shader" << std::endl;
    return false;
  }

  discShader = initShader("src/shaders/disc.vsh", "src/shaders/disc.fsh");
  if(discShader == 0){
    std::cerr << "Failed to initialise Discontinuity shader" << std::endl;
    return false;
  }

  std::ifstream file;
  file.open("src/kernels/kernel.cl");
  if(!file.is_open()){
    std::cerr << "Failed to open kernel.cl" << std::endl;
    return false;
  }
  std::filebuf* rdbuf = file.rdbuf();
  std::size_t size = rdbuf->pubseekoff(0, file.end, file.in);
  rdbuf->pubseekpos(0, file.in);
  char* buffer = new char[size+1];
  rdbuf->sgetn(buffer, size);
  buffer[size] = '\0';
  file.close();

  noOfVPLS = config.GetInteger("renderer", "noOfVPLs", 1);
  maxVPLGenPerFrame = config.GetInteger("renderer", "maxVPLGenPerFrame", 5);
  interleavedSamplingSize = config.GetInteger("renderer", "interleavedSamplingSize", 5);
  glGenTextures(1, &vMasks);
  glBindTexture(GL_TEXTURE_2D_ARRAY, vMasks);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, p_width, p_height, noOfVPLS, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  clMasks = clCreateFromGLTexture(clContext, CL_MEM_READ_WRITE, GL_TEXTURE_2D_ARRAY, 0, vMasks, NULL);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  clProgram = clCreateProgramWithSource(clContext, 1, (const char**)&buffer, NULL, NULL);
  clBuildProgram(clProgram, 1, devices, NULL, NULL, &clErr);
  cl_int initErr;
  clInitMasksKernel = clCreateKernel(clProgram, "init_masks", &initErr);
  cl_int preErr;
  clPreRaysKernel = clCreateKernel(clProgram, "pre_rays", &preErr);
  cl_int postErr;
  clPostRaysKernel = clCreateKernel(clProgram, "post_rays", &postErr);
  if(initErr != 0 || preErr != 0 || postErr != 0){
  	char buildLog[LOG_MESSAGE_LENGTH];
  	clGetProgramBuildInfo(clProgram, devices[0], CL_PROGRAM_BUILD_LOG, LOG_MESSAGE_LENGTH, buildLog, NULL);
   	std::cerr << "Failed to build OpenCL Kernel : " << std::endl << buildLog << std::endl;
   	return false;
  }
  clPositions = clCreateFromGLTexture(clContext, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, gPosition, &clErr);
  //clNormals = clCreateFromGLTexture(clContext, CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, gNormal, &clErr);
  //clSpeculars = clCreateFromGLTexture(clContext, CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, gSpecular, &clErr);
  clRays = clCreateBuffer(clContext, CL_MEM_READ_WRITE, noOfVPLS * p_width * p_height * sizeof(RR::ray), NULL, NULL);
  clVPLs = clCreateBuffer(clContext, CL_MEM_READ_WRITE, noOfVPLS * sizeof(Light), NULL, NULL);
  //clIsects = clCreateBuffer(clContext, CL_MEM_READ_WRITE, noOfVPLS * p_width * p_height * sizeof(RR::Intersection), NULL, NULL);
  clOcclus = clCreateBuffer(clContext, CL_MEM_READ_WRITE, noOfVPLS * p_width * p_height * sizeof(int), NULL, NULL);

  rrRays = RR::CreateFromOpenClBuffer(intersectionApi, clRays);
  //rrIsects = RR::CreateFromOpenClBuffer(intersectionApi, clIsects);
  rrOcclus = RR::CreateFromOpenClBuffer(intersectionApi, clOcclus);

  rDelta = config.GetReal("renderer", "rDelta", 0.1f);
  for(int i = 0; i < noOfVPLS; ++i){
    vpls.push_back(pl);
  	validVPLs.push_back(false);
  }
  noOfInvalidVPLs = noOfVPLS;
  
  //vplRays.reserve(noOfVPLS);
  vplIsects.reserve(noOfVPLS);
  vplOcclus.reserve(noOfVPLS);
  vplRayBuffer = intersectionApi->CreateBuffer(noOfVPLS * sizeof(RR::ray), nullptr);
  vplIsectBuffer = intersectionApi->CreateBuffer(noOfVPLS * sizeof(RR::Intersection), nullptr);
  vplOccluBuffer = intersectionApi->CreateBuffer(noOfVPLS * sizeof(int), nullptr);

	vplNo = 0;
	currVPL = 0;

	glGenFramebuffers(1, &dBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, dBuffer);
	glGenTextures(1, &dColor);
	glBindTexture(GL_TEXTURE_2D, dColor);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, p_width, p_height, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dColor, 0);

	unsigned int dAttachments[1] = {GL_COLOR_ATTACHMENT0};
	glDrawBuffers(1, dAttachments);

 	glGenFramebuffers(1, &iBuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, iBuffer);
  glGenTextures(1, &iColor);
  glBindTexture(GL_TEXTURE_2D, iColor);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, p_width, p_height, 0, GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, iColor, 0);

  glDrawBuffers(1, dAttachments);

  glGenFramebuffers(1, &discBuffer1);
  glBindFramebuffer(GL_FRAMEBUFFER, discBuffer1);
  
  glGenTextures(1, &discIndirect1);
  glBindTexture(GL_TEXTURE_2D, discIndirect1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, p_width, p_height, 0, GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, discIndirect1, 0);
  
  glDrawBuffers(1, attachments);

  glGenFramebuffers(1, &discBuffer2);
  glBindFramebuffer(GL_FRAMEBUFFER, discBuffer2);
  
  glGenTextures(1, &discIndirect2);
  glBindTexture(GL_TEXTURE_2D, discIndirect2);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, p_width, p_height, 0, GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, discIndirect2, 0);
  
  glDrawBuffers(1, attachments);
   
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
   
  lightSpeed = config.GetReal("renderer", "lightSpeed", 1.f);

  iHistoryIndex = 0;
  iHistorySize = config.GetInteger("renderer", "iHistorySize", 1);
  viewHistory.reserve(iHistorySize);
  glGenTextures(1, &iHistory);  
  glBindTexture(GL_TEXTURE_2D_ARRAY, iHistory);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB16F, p_width, p_height, iHistorySize, 0, GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glGenTextures(1, &pHistory);  
  glBindTexture(GL_TEXTURE_2D_ARRAY, pHistory);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA16F, p_width, p_height, iHistorySize, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
  
  iHistoryShader = initShader("src/shaders/ihistory.vsh", "src/shaders/ihistory.fsh");
  if(iHistoryShader == 0){
    std::cerr << "Failed to initialise iHistory shader" << std::endl;
    return false;
  }

  size_t global_item_size[3] = {p_width, p_height, noOfVPLS};
  size_t local_item_size[3] = {1, 1, 1};
  clSetKernelArg(clInitMasksKernel, 0, sizeof(cl_mem), (void *)&clMasks);
  clEnqueueNDRangeKernel(clQueue, clInitMasksKernel, 3, NULL, global_item_size, local_item_size, 0, NULL, NULL);
  
  areaLightChance = config.GetReal("renderer", "AreaLightChance", 0.1f);
  lightRadius = config.GetReal("renderer", "LightRadius", 0.1f);
  noOfVPLBounces = config.GetInteger("renderer", "noOfVPLBounces", 0.1f);
  
  glClearColor(0.f, 0.f, 0.f, 1.0f);
  return true;
}

RR::IntersectionApi* renderer::getIntersectionApi(){
  return intersectionApi;
}

void renderer::update(float deltaTime){
	float step = lightSpeed * deltaTime;
	if(i) pl.position += step * glm::vec4(0, 1, 0, 0);
	if(k) pl.position -= step * glm::vec4(0, 1, 0, 0);
	if(j) pl.position -= step * glm::vec4(1, 0, 0, 0);
	if(l) pl.position += step * glm::vec4(1, 0, 0, 0);
	if(o) pl.position -= step * glm::vec4(0, 0, 1, 0);
	if(u) pl.position += step * glm::vec4(0, 0, 1, 0);
	if(i || k || j || l || o || u) vplUpdated = true;

  if(indirectEnabled){
		int lihi = iHistoryIndex - 1;
		if(lihi < 0) lihi = iHistorySize;
		lihi = iHistoryIndex % iHistorySize;
	  if(noOfInvalidVPLs < maxVPLGenPerFrame){
		  for(int i = 0; i < noOfVPLS / iHistorySize ; ++i){
		    if(validVPLs[(lihi * noOfVPLS / iHistorySize) + i]){
					validVPLs[(lihi * noOfVPLS / iHistorySize) + i] = false;
					noOfInvalidVPLs++;
				}
		  }
	  }
  	
	  for(int i = 0; i < vpls.size(); ++i){
			Light pvpl = pl;
			if(i > noOfVPLS / noOfVPLBounces)
				pvpl = vpls[i - (noOfVPLS / noOfVPLBounces)];
	  	Light vpl = vpls[i];
	  	RR::ray r;
	  	glm::vec4 diff = vpl.position - pvpl.position;
	  	r.o = RR::float4(pvpl.position.x, pvpl.position.y, pvpl.position.z, glm::length(diff) - rDelta);
	  	diff = glm::normalize(diff);
	  	r.d = RR::float4(diff.x, diff.y, diff.z, 0.f);
	  	vplRays[i] = r;
	  }
	
	  RR::Buffer* ray_buffer = intersectionApi->CreateBuffer(vpls.size() * sizeof(RR::ray), &vplRays);
    RR::Buffer* occlu_buffer = intersectionApi->CreateBuffer(vpls.size() * sizeof(int), nullptr);

    intersectionApi->QueryOcclusion(ray_buffer, vpls.size(), occlu_buffer, nullptr, nullptr);

    int* occlus = nullptr;
    RR::Event* e = nullptr;
    intersectionApi->MapBuffer(occlu_buffer, RR::kMapRead, 0, vpls.size() * sizeof(int), (void**)&occlus, &e);

    e->Wait();
    intersectionApi->DeleteEvent(e);
	  e = nullptr;

	  for(int i = vpls.size() - 1; i >= 0; --i){
			if(occlus[i] != -1){
				int j = i;
				if(j < noOfVPLS && validVPLs[j]){
					validVPLs[j] = false;
					noOfInvalidVPLs++;
					j += (noOfVPLS / noOfVPLBounces);
				}
			}
	  }

    intersectionApi->DeleteBuffer(occlu_buffer);
    intersectionApi->DeleteBuffer(ray_buffer);
		
		unsigned int noOfVPLSShot = 0;
		unsigned int noOfVPLSTried = 0;
		while(noOfVPLSShot < maxVPLGenPerFrame && noOfVPLSTried < noOfVPLS){
			noOfVPLSTried++;
			currVPL = (currVPL + 1) % noOfVPLS;
			if(!validVPLs[currVPL]){
				Light pvpl = pl;
				double* hltn = halton(1000 + vplNo++, 3);
				double* chance = halton(1000 + vplNo, 1);
				RR::ray r;
				r.extra.x = currVPL;
				r.extra.y = -1;
				r.o = RR::float4(pvpl.position.x, pvpl.position.y, pvpl.position.z, 1000.f);
				r.d = RR::float3(2*hltn[0] - 1, 2*hltn[1] - 1, 2*hltn[2] - 1);				
				if(currVPL >= noOfVPLS / noOfVPLBounces){
				  r.extra.y = currVPL - (noOfVPLS / noOfVPLBounces);
				  if(!validVPLs[r.extra.y]){
				  	continue;
				  }
			 		pvpl = vpls[r.extra.y];
					glm::vec3 dir = glm::vec3(2*hltn[0] - 1, 2*hltn[1] - 1, 2*hltn[2] - 1);
					glm::vec3 normal = glm::vec3(pvpl.normal.x, pvpl.normal.y, pvpl.normal.z);
					dir = glm::faceforward(-dir, dir, normal);
					r.o = RR::float4(pvpl.position.x, pvpl.position.y, pvpl.position.z, 1000.f);
					r.d = RR::float3(dir.x, dir.y, dir.z);
			 	}
				vplRays[noOfVPLSShot] = r;
				noOfVPLSShot++;
			}
		}

		if(noOfVPLSShot > 0){	
			RR::Buffer* ray_buffer = intersectionApi->CreateBuffer(noOfVPLSShot * sizeof(RR::ray), &vplRays);
	    RR::Buffer* isect_buffer = intersectionApi->CreateBuffer(noOfVPLSShot * sizeof(RR::Intersection), nullptr);
	    intersectionApi->QueryIntersection(ray_buffer, noOfVPLSShot, isect_buffer, nullptr, nullptr);
	
	    RR::Event* e = nullptr;
	    RR::Intersection* isects = nullptr;
	    intersectionApi->MapBuffer(isect_buffer, RR::kMapRead, 0, noOfVPLSShot * sizeof(RR::Intersection), (void**)&isects, &e);
	
	    e->Wait();
	    intersectionApi->DeleteEvent(e);
			e = nullptr;
		
		
	    for(int i = 0; i < noOfVPLSShot; ++i){
	      RR::ray ray = vplRays[i];
	      RR::Intersection isect = isects[i];
				if(isect.shapeid != -1){
	      	Light pvpl = pl;
	      	int vplIndex = ray.extra.x;
	      	if(ray.extra.y != -1){
	      		pvpl = vpls[ray.extra.y];
	      	}
	        Light vpl;
	        float distance = isect.uvwt.w;
	        glm::vec4 normal = Model::getNormal(isect.shapeid, isect.primid, isect.uvwt.x, isect.uvwt.y);
					vpl.normal = normal;
	        vpl.position = glm::vec4(
	          pvpl.position.x + (distance * ray.d.x) + (normal.x * rDelta),
	          pvpl.position.y + (distance * ray.d.y) + (normal.y * rDelta),
	          pvpl.position.z + (distance * ray.d.z) + (normal.z * rDelta),
	          1
	        );
	        glm::vec4 incident = glm::normalize(pvpl.position - vpl.position);
	        vpl.diffuse = Model::getDiffuse(isect.shapeid, isect.primid, isect.uvwt.x, isect.uvwt.y);
	        vpl.diffuse *= pvpl.diffuse * glm::max(glm::dot(normal, incident), 0.f);// / (PI);
	        vpl.specular = glm::vec4(0, 0, 0, 1);
	        if(ray.extra.y != -1){
	        	float dist = distance;
	        	for(int j = ray.extra.y; j >= noOfVPLS / noOfVPLBounces; j -= noOfVPLS / noOfVPLBounces){
	        		int k = j - (noOfVPLS / noOfVPLBounces);
	        		dist += glm::distance(vpls[j].position, vpls[k].position);
	        	}
					  float attenuation = 1 / (1 + dist * dist);
					  vpl.diffuse *= attenuation;
					  vpl.specular *= attenuation;
	        }
		 			vpls[vplIndex] = vpl;
		 			validVPLs[vplIndex] = true;
		 			noOfInvalidVPLs--;
	      }
	    }

	    intersectionApi->DeleteBuffer(isect_buffer);
      intersectionApi->DeleteBuffer(ray_buffer);
	
	    clEnqueueWriteBuffer(clQueue, clVPLs, CL_TRUE, 0, vpls.size() * sizeof(Light), vpls.data(), 0, NULL, NULL);
	    
			vplUpdated = true;
	  }
  }

  if(vplDebugEnabled && vplUpdated){
  	std::vector<Light> vpls2;
		for(int i = 0; i < vpls.size(); ++i){
			Light pvpl = pl;	
	    if(i >= noOfVPLS / noOfVPLBounces){
	    	pvpl = vpls[i - (noOfVPLS / noOfVPLBounces)];
			}
			vpls2.push_back(pvpl);
			vpls2.push_back(vpls[i]);		
		}
	
		glGenBuffers(1, &vpl_vbo);
		glGenVertexArrays(1, &vpl_vao);
		glBindVertexArray(vpl_vao);
		glBindBuffer(GL_ARRAY_BUFFER, vpl_vbo);
		glBufferData(GL_ARRAY_BUFFER, vpls2.size() * sizeof(Light), vpls2.data(), GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Light), (void*)offsetof(Light, position));
		glBindVertexArray(0);
		vplUpdated = false;
	}

  if(directEnabled){
		glm::vec3 plpos = glm::vec3(pl.position.x, pl.position.y, pl.position.z);
		dpth_trnsfrms[0] = dpth_prjctn * glm::lookAt(plpos, plpos + glm::vec3( 1, 0, 0), glm::vec3( 0,-1, 0));
		dpth_trnsfrms[1] = dpth_prjctn * glm::lookAt(plpos, plpos + glm::vec3(-1, 0, 0), glm::vec3( 0,-1, 0));
		dpth_trnsfrms[2] = dpth_prjctn * glm::lookAt(plpos, plpos + glm::vec3( 0, 1, 0), glm::vec3( 0, 0, 1));
		dpth_trnsfrms[3] = dpth_prjctn * glm::lookAt(plpos, plpos + glm::vec3( 0,-1, 0), glm::vec3( 0, 0,-1));
		dpth_trnsfrms[4] = dpth_prjctn * glm::lookAt(plpos, plpos + glm::vec3( 0, 0, 1), glm::vec3( 0,-1, 0));
		dpth_trnsfrms[5] = dpth_prjctn * glm::lookAt(plpos, plpos + glm::vec3( 0, 0,-1), glm::vec3( 0,-1, 0));

		glViewport(0, 0, dpth_width, dpth_height);
		glBindFramebuffer(GL_FRAMEBUFFER, dpth_fbo);
		glClear(GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glUseProgram(dpth_shader);

		glUniformMatrix4fv(glGetUniformLocation(dpth_shader, "shadowMatrices[0]"), 1, GL_FALSE, &dpth_trnsfrms[0][0][0]);
		glUniformMatrix4fv(glGetUniformLocation(dpth_shader, "shadowMatrices[1]"), 1, GL_FALSE, &dpth_trnsfrms[1][0][0]);
		glUniformMatrix4fv(glGetUniformLocation(dpth_shader, "shadowMatrices[2]"), 1, GL_FALSE, &dpth_trnsfrms[2][0][0]);
		glUniformMatrix4fv(glGetUniformLocation(dpth_shader, "shadowMatrices[3]"), 1, GL_FALSE, &dpth_trnsfrms[3][0][0]);
		glUniformMatrix4fv(glGetUniformLocation(dpth_shader, "shadowMatrices[4]"), 1, GL_FALSE, &dpth_trnsfrms[4][0][0]);
		glUniformMatrix4fv(glGetUniformLocation(dpth_shader, "shadowMatrices[5]"), 1, GL_FALSE, &dpth_trnsfrms[5][0][0]);
		glUniform3fv(glGetUniformLocation(dpth_shader, "lightPos"), 1, &pl.position[0]);
		glUniform1f(glGetUniformLocation(dpth_shader, "far_plane"), dpth_far_plane);
		Model::draw(dpth_shader);
  }
  glViewport(0, 0, p_width, p_height);
  glm::mat4 view = Camera::getViewMatrix();
  glm::vec3 position = Camera::getPosition();
  glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glUseProgram(gBufferShader);
  glUniformMatrix4fv(glGetUniformLocation(gBufferShader, "view"), 1, GL_FALSE, &view[0][0]);
  glUniformMatrix4fv(glGetUniformLocation(gBufferShader, "projection"), 1, GL_FALSE, &projection[0][0]);
  Model::draw(gBufferShader);

  if(directEnabled){
	  glBindFramebuffer(GL_FRAMEBUFFER, dBuffer);
	  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	  glDisable(GL_DEPTH_TEST);
	  glUseProgram(dPlaneShader);
	  glUniform3fv(glGetUniformLocation(dPlaneShader, "light.position"), 1, &pl.position[0]);
	  glUniform3fv(glGetUniformLocation(dPlaneShader, "light.diffuse"), 1, &pl.diffuse[0]);
	  glUniform3fv(glGetUniformLocation(dPlaneShader, "light.specular"), 1, &pl.specular[0]);
	  glUniform3fv(glGetUniformLocation(dPlaneShader, "viewPos"), 1, &position[0]);
	  glUniform1f(glGetUniformLocation(dPlaneShader, "far_plane"), dpth_far_plane);
	  glUniform1i(glGetUniformLocation(dPlaneShader, "gPosition"), 0);
	  glUniform1i(glGetUniformLocation(dPlaneShader, "gNormal"), 1);
	  glUniform1i(glGetUniformLocation(dPlaneShader, "gAlbedo"), 2);
	  glUniform1i(glGetUniformLocation(dPlaneShader, "gSpecular"), 3);
	  glUniform1i(glGetUniformLocation(dPlaneShader, "dMap"), 4);
	  glActiveTexture(GL_TEXTURE0);
	  glBindTexture(GL_TEXTURE_2D, gPosition);
	  glActiveTexture(GL_TEXTURE1);
	  glBindTexture(GL_TEXTURE_2D, gNormal);
	  glActiveTexture(GL_TEXTURE2);
	  glBindTexture(GL_TEXTURE_2D, gAlbedo);
	  glActiveTexture(GL_TEXTURE3);
	  glBindTexture(GL_TEXTURE_2D, gSpecular);
	  glActiveTexture(GL_TEXTURE4);
	  glBindTexture(GL_TEXTURE_CUBE_MAP, dpth_cbmp);
	  glBindVertexArray(dPlaneVAO);
	  glDrawArrays(GL_TRIANGLES, 0, 6);
  }

  if(indirectEnabled && vpls.size() > 0){
	  glFinish();
	  clEnqueueAcquireGLObjects(clQueue, 1, &clPositions, 0, 0, NULL);
	  clEnqueueAcquireGLObjects(clQueue, 1, &clMasks, 0, 0, NULL);

	  float realVPP = vpls.size()/(float)(interleavedSamplingSize*interleavedSamplingSize*iHistorySize);
	  unsigned int vplsPerPixel = realVPP;
	  size_t global_item_size[3] = {p_width, p_height, vplsPerPixel};
	  
	  if(realVPP < 1){
	  	global_item_size[0] = p_width * realVPP;
	  	global_item_size[1] = p_height * realVPP;
	  	global_item_size[2] = 1;
	  	vplsPerPixel = 1;
	  }else{
	  	realVPP = 1;
	  }
	  size_t local_item_size[3] = {1, 1, 1};
	  
	  clSetKernelArg(clPreRaysKernel, 0, sizeof(cl_mem), (void *)&clPositions);
	  clSetKernelArg(clPreRaysKernel, 1, sizeof(cl_mem), (void *)&clVPLs);
	  clSetKernelArg(clPreRaysKernel, 2, sizeof(unsigned int), &vplsPerPixel);
	  clSetKernelArg(clPreRaysKernel, 3, sizeof(float), &realVPP);
	  clSetKernelArg(clPreRaysKernel, 4, sizeof(unsigned int), &p_width);
	  clSetKernelArg(clPreRaysKernel, 5, sizeof(unsigned int), &interleavedSamplingSize);
	  clSetKernelArg(clPreRaysKernel, 6, sizeof(unsigned int), &iHistoryIndex);
	  clSetKernelArg(clPreRaysKernel, 7, sizeof(unsigned int), &iHistorySize);
	  clSetKernelArg(clPreRaysKernel, 8, sizeof(cl_mem), (void *)&clRays);

	  clEnqueueNDRangeKernel(clQueue, clPreRaysKernel, 3, NULL, global_item_size, local_item_size, 0, NULL, NULL);
		  
	  intersectionApi->QueryOcclusion(rrRays, global_item_size[0] * global_item_size[1] * global_item_size[2], rrOcclus, nullptr, nullptr);
	
	  clSetKernelArg(clPostRaysKernel, 0, sizeof(cl_mem), (void *)&clRays);
	  clSetKernelArg(clPostRaysKernel, 1, sizeof(cl_mem), (void *)&clOcclus);
	  clSetKernelArg(clPostRaysKernel, 2, sizeof(unsigned int), &vplsPerPixel);
	  clSetKernelArg(clPostRaysKernel, 3, sizeof(float), &realVPP);
	  clSetKernelArg(clPostRaysKernel, 4, sizeof(unsigned int), &p_width);
	  clSetKernelArg(clPostRaysKernel, 5, sizeof(unsigned int), &interleavedSamplingSize);
	  clSetKernelArg(clPostRaysKernel, 6, sizeof(unsigned int), &iHistoryIndex);
	  clSetKernelArg(clPostRaysKernel, 7, sizeof(unsigned int), &iHistorySize);
	  clSetKernelArg(clPostRaysKernel, 8, sizeof(cl_mem), (void *)&clVPLs);
  	clSetKernelArg(clPostRaysKernel, 9, sizeof(cl_mem), (void *)&clMasks);
	  	  
	  clEnqueueNDRangeKernel(clQueue, clPostRaysKernel, 3, NULL, global_item_size, local_item_size, 0, NULL, NULL);

	  clEnqueueReleaseGLObjects(clQueue, 1, &clMasks, 0, 0, NULL);
	  clEnqueueReleaseGLObjects(clQueue, 1, &clPositions, 0, 0, NULL);
	  clFinish(clQueue);
	  
	  glBindFramebuffer(GL_FRAMEBUFFER, iBuffer);
	  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	  glDisable(GL_DEPTH_TEST);
	  glUseProgram(iPlaneShader);
	  glUniform3fv(glGetUniformLocation(iPlaneShader, "viewPos"), 1, &position[0]);
	  glUniform1i(glGetUniformLocation(iPlaneShader, "gPosition"), 0);
	  glUniform1i(glGetUniformLocation(iPlaneShader, "gNormal"), 1);
	  glUniform1i(glGetUniformLocation(iPlaneShader, "gSpecular"), 3);
	  glUniform3fv(glGetUniformLocation(iPlaneShader, "pl.position"), 1, &pl.position[0]);
	  glUniform3fv(glGetUniformLocation(iPlaneShader, "pl.diffuse"), 1, &pl.diffuse[0]);
	  glUniform3fv(glGetUniformLocation(iPlaneShader, "pl.specular"), 1, &pl.specular[0]);
	  for(int i = 0; i < noOfVPLS; ++i){
	    glUniform3fv(glGetUniformLocation(iPlaneShader, ("vpls[" + std::to_string(i) + "].position").c_str()), 1, &vpls[i].position[0]);
	    glUniform3fv(glGetUniformLocation(iPlaneShader, ("vpls[" + std::to_string(i) + "].diffuse").c_str()), 1, &vpls[i].diffuse[0]);
	    glUniform3fv(glGetUniformLocation(iPlaneShader, ("vpls[" + std::to_string(i) + "].specular").c_str()), 1, &vpls[i].specular[0]);
	  }
	  glUniform1i(glGetUniformLocation(iPlaneShader, "vplMasks"), 4);
	  glUniform1i(glGetUniformLocation(iPlaneShader, "debugVPLI"), debugVPL);
	  glUniform1i(glGetUniformLocation(iPlaneShader, "noOfVPLs"), vpls.size());
	  glUniform1i(glGetUniformLocation(iPlaneShader, "iHistorySize"), iHistorySize);
	  glUniform1i(glGetUniformLocation(iPlaneShader, "iHistoryIndex"), iHistoryIndex);
	  glUniform1i(glGetUniformLocation(iPlaneShader, "noOfVPLBounces"), noOfVPLBounces);
	  glActiveTexture(GL_TEXTURE0);
	  glBindTexture(GL_TEXTURE_2D, gPosition);
	  glActiveTexture(GL_TEXTURE1);
	  glBindTexture(GL_TEXTURE_2D, gNormal);
	  glActiveTexture(GL_TEXTURE3);
	  glBindTexture(GL_TEXTURE_2D, gSpecular);
	  glActiveTexture(GL_TEXTURE4);
	  glBindTexture(GL_TEXTURE_2D_ARRAY, vMasks);
	  glBindVertexArray(dPlaneVAO);
	  glDrawArrays(GL_TRIANGLES, 0, 6);

	  glBindFramebuffer(GL_FRAMEBUFFER, discBuffer1);
	  glUseProgram(discShader);
	  glUniform1i(glGetUniformLocation(discShader, "gNormal"), 0);
	  glUniform1i(glGetUniformLocation(discShader, "gIndirect"), 1);
	  glUniform1i(glGetUniformLocation(discShader, "gPosition"), 2);
	  glUniform1i(glGetUniformLocation(discShader, "pass"), 1);
	  glActiveTexture(GL_TEXTURE0);
	  glBindTexture(GL_TEXTURE_2D, gNormal);
	  glActiveTexture(GL_TEXTURE1);
	  glBindTexture(GL_TEXTURE_2D, iColor);
	  glActiveTexture(GL_TEXTURE2);
	  glBindTexture(GL_TEXTURE_2D, gPosition);
	  glBindVertexArray(dPlaneVAO);
	  glDrawArrays(GL_TRIANGLES, 0, 6);
	
	  glBindFramebuffer(GL_FRAMEBUFFER, discBuffer2);
	  glUniform1i(glGetUniformLocation(discShader, "pass"), 2);
	  glActiveTexture(GL_TEXTURE1);
	  glBindTexture(GL_TEXTURE_2D, discIndirect1);
	  glBindVertexArray(dPlaneVAO);
	  glDrawArrays(GL_TRIANGLES, 0, 6);

	  iHistoryIndex = (iHistoryIndex + 1) % iHistorySize;
	  glCopyImageSubData(discIndirect2, GL_TEXTURE_2D, 0, 0, 0, 0, iHistory, GL_TEXTURE_2D_ARRAY, 0, 0, 0, iHistoryIndex, p_width, p_height, 1);
	  glCopyImageSubData(gPosition, GL_TEXTURE_2D, 0, 0, 0, 0, pHistory, GL_TEXTURE_2D_ARRAY, 0, 0, 0, iHistoryIndex, p_width, p_height, 1);
	  viewHistory[iHistoryIndex] = view;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glUseProgram(iHistoryShader);
  glClear(GL_COLOR_BUFFER_BIT);
  for(int i = 0; i < iHistorySize; ++i){
  	glUniformMatrix4fv(glGetUniformLocation(iHistoryShader, ("vHistory[" + std::to_string(i) + "]").c_str()), 1, GL_FALSE, &viewHistory[i][0][0]);
  }
  glUniformMatrix4fv(glGetUniformLocation(iHistoryShader, "projection"), 1, GL_FALSE, &projection[0][0]);
  glUniform1i(glGetUniformLocation(iHistoryShader, "iHistorySize"), iHistorySize);
  glUniform1i(glGetUniformLocation(iHistoryShader, "iHistoryIndex"), iHistoryIndex);
  glUniform1i(glGetUniformLocation(iHistoryShader, "dEnabled"), directEnabled);
  glUniform1i(glGetUniformLocation(iHistoryShader, "iEnabled"), indirectEnabled);
  glUniform1i(glGetUniformLocation(iHistoryShader, "iHistory"), 0);
  glUniform1i(glGetUniformLocation(iHistoryShader, "pHistory"), 1);
  glUniform1i(glGetUniformLocation(iHistoryShader, "dColor"), 2);
  glUniform1i(glGetUniformLocation(iHistoryShader, "gAlbedo"), 3);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, iHistory);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D_ARRAY, pHistory);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, dColor);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, gAlbedo);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  if(vplDebugEnabled){
	  glUseProgram(vpl_shader);
	  glUniformMatrix4fv(glGetUniformLocation(vpl_shader, "model"), 1, GL_FALSE, &Model::getModelMatrix()[0][0]);
	  glUniformMatrix4fv(glGetUniformLocation(vpl_shader, "view"), 1, GL_FALSE, &view[0][0]);
	  glUniformMatrix4fv(glGetUniformLocation(vpl_shader, "projection"), 1, GL_FALSE, &projection[0][0]);
	  glUniform1i(glGetUniformLocation(vpl_shader, "debugVPLI"), debugVPL);
	  glBindVertexArray(vpl_vao);
	  glDrawArrays(GL_LINES, 0, vpls.size() * 2);
  }
}

void renderer::destroy(){

}
