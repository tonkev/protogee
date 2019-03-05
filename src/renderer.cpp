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

struct Light{
  glm::vec3 position;
  glm::vec3 diffuse;
  glm::vec3 specular;
};

Light pl;
std::vector<Light> vpls;

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
cl_kernel clPreRaysKernel;
cl_kernel clPostRaysKernel;

RR::Buffer* rrRays;
RR::Buffer* rrIsects;
RR::Buffer* rrOcclus;

float rDelta;

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

  p_width = config.GetInteger("interface", "width", 480);
  p_height = config.GetInteger("interface", "height", 320);

//  model = glm::mat4(1.0f);
  //model = glm::scale(model, glm::vec3(0.1f));
  projection = glm::perspective(glm::radians(45.0f), p_width / float(p_height), 0.1f, 10000.0f);

  pl.position = glm::vec3(0, 10, 0);
  pl.position.y = config.GetReal("renderer", "LightY", 1);
  float lightPower = config.GetReal("renderer", "LightPower", 1);
  pl.diffuse = glm::vec3(lightPower);
  pl.specular = glm::vec3(lightPower);

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
  dpth_trnsfrms.push_back(dpth_prjctn * glm::lookAt(pl.position, pl.position + glm::vec3( 1, 0, 0), glm::vec3( 0,-1, 0)));
  dpth_trnsfrms.push_back(dpth_prjctn * glm::lookAt(pl.position, pl.position + glm::vec3(-1, 0, 0), glm::vec3( 0,-1, 0)));
  dpth_trnsfrms.push_back(dpth_prjctn * glm::lookAt(pl.position, pl.position + glm::vec3( 0, 1, 0), glm::vec3( 0, 0, 1)));
  dpth_trnsfrms.push_back(dpth_prjctn * glm::lookAt(pl.position, pl.position + glm::vec3( 0,-1, 0), glm::vec3( 0, 0,-1)));
  dpth_trnsfrms.push_back(dpth_prjctn * glm::lookAt(pl.position, pl.position + glm::vec3( 0, 0, 1), glm::vec3( 0,-1, 0)));
  dpth_trnsfrms.push_back(dpth_prjctn * glm::lookAt(pl.position, pl.position + glm::vec3( 0, 0,-1), glm::vec3( 0,-1, 0)));

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
  glBindTexture(GL_TEXTURE_2D, gPosition);
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
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, p_width, p_height, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedo, 0);

  glGenTextures(1, &gSpecular);
  glBindTexture(GL_TEXTURE_2D, gSpecular);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, p_width, p_height, 0, GL_RED, GL_FLOAT, NULL);
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
  glGenTextures(1, &vMasks);
  glBindTexture(GL_TEXTURE_2D_ARRAY, vMasks);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R8, p_width, p_height, noOfVPLS, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  clMasks = clCreateFromGLTexture(clContext, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D_ARRAY, 0, vMasks, NULL);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  clProgram = clCreateProgramWithSource(clContext, 1, (const char**)&buffer, NULL, NULL);
  clBuildProgram(clProgram, 1, devices, NULL, NULL, &clErr);
  cl_int preErr;
  clPreRaysKernel = clCreateKernel(clProgram, "pre_rays", &preErr);
  cl_int postErr;
  clPostRaysKernel = clCreateKernel(clProgram, "post_rays", &postErr);
  if(preErr != 0 || postErr != 0){
  	char buildLog[LOG_MESSAGE_LENGTH];
  	clGetProgramBuildInfo(clProgram, devices[0], CL_PROGRAM_BUILD_LOG, LOG_MESSAGE_LENGTH, buildLog, NULL);
   	std::cerr << "Failed to build OpenCL Kernel : " << std::endl << buildLog << std::endl;
   	return false;
  }
  clPositions = clCreateFromGLTexture(clContext, CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, gPosition, &clErr);
  //clNormals = clCreateFromGLTexture(clContext, CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, gNormal, &clErr);
  //clSpeculars = clCreateFromGLTexture(clContext, CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, gSpecular, &clErr);
  clRays = clCreateBuffer(clContext, CL_MEM_READ_WRITE, noOfVPLS * p_width * p_height * sizeof(RR::ray), NULL, NULL);
  clVPLs = clCreateBuffer(clContext, CL_MEM_READ_WRITE, noOfVPLS * sizeof(Light), NULL, NULL);
  clIsects = clCreateBuffer(clContext, CL_MEM_READ_WRITE, noOfVPLS * p_width * p_height * sizeof(RR::Intersection), NULL, NULL);
  clOcclus = clCreateBuffer(clContext, CL_MEM_READ_WRITE, noOfVPLS * p_width * p_height * sizeof(int), NULL, NULL);

  clSetKernelArg(clPreRaysKernel, 0, sizeof(cl_mem), (void *)&clPositions);
  clSetKernelArg(clPreRaysKernel, 1, sizeof(cl_mem), (void *)&clVPLs);
  clSetKernelArg(clPreRaysKernel, 2, sizeof(unsigned int), &noOfVPLS);
  clSetKernelArg(clPreRaysKernel, 3, sizeof(unsigned int), &p_width);
  clSetKernelArg(clPreRaysKernel, 4, sizeof(cl_mem), (void *)&clRays);

  clSetKernelArg(clPostRaysKernel, 0, sizeof(cl_mem), (void *)&clRays);
  //clSetKernelArg(clPostRaysKernel, 1, sizeof(cl_mem), (void *)&clIsects);
  clSetKernelArg(clPostRaysKernel, 1, sizeof(cl_mem), (void *)&clOcclus);
  clSetKernelArg(clPostRaysKernel, 2, sizeof(unsigned int), &noOfVPLS);
  clSetKernelArg(clPostRaysKernel, 3, sizeof(unsigned int), &p_width);
  clSetKernelArg(clPostRaysKernel, 4, sizeof(cl_mem), (void *)&clMasks);

  rrRays = RR::CreateFromOpenClBuffer(intersectionApi, clRays);
  rrIsects = RR::CreateFromOpenClBuffer(intersectionApi, clIsects);
  rrOcclus = RR::CreateFromOpenClBuffer(intersectionApi, clOcclus);

  rDelta = config.GetReal("renderer", "rDelta", 0.1f);

  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

  return true;
}

RR::IntersectionApi* renderer::getIntersectionApi(){
  return intersectionApi;
}

void renderer::update(){
  if(vpls.empty()){
    RR::ray rays[noOfVPLS];
    for(int i = 0; i < noOfVPLS; ++i){
      double* hltn = halton(1000 + i, 3);
      rays[i].o = RR::float4(pl.position.x, pl.position.y, pl.position.z, 1000.f);
      rays[i].d = RR::float3(2*hltn[0] - 1, 2*hltn[1] - 1, 2*hltn[2] - 1);
    }
    RR::Buffer* ray_buffer = intersectionApi->CreateBuffer(noOfVPLS * sizeof(RR::ray), rays);
    RR::Buffer* isect_buffer = intersectionApi->CreateBuffer(noOfVPLS * sizeof(RR::Intersection), nullptr);
    intersectionApi->QueryIntersection(ray_buffer, noOfVPLS, isect_buffer, nullptr, nullptr);

    RR::Event* e = nullptr;
    RR::Intersection* isects = nullptr;
    intersectionApi->MapBuffer(isect_buffer, RR::kMapRead, 0, noOfVPLS * sizeof(RR::Intersection), (void**)&isects, &e);

    e->Wait();
    intersectionApi->DeleteEvent(e);
    e = nullptr;

    for(int i = 0; i < noOfVPLS; ++i){
      RR::ray ray = rays[i];
      RR::Intersection isect = isects[i];
      if(isect.shapeid != -1){
        Light vpl;
        float distance = isect.uvwt.w - rDelta;
        vpl.position = glm::vec3(
          pl.position.x + distance * ray.d.x,
          pl.position.y + distance * ray.d.y,
          pl.position.z + distance * ray.d.z
        );
        vpl.diffuse = Model::getDiffuse(isect.shapeid, isect.primid, isect.uvwt.x, isect.uvwt.y);
        vpl.diffuse = glm::vec3(vpl.diffuse.x * pl.diffuse.x, vpl.diffuse.y * pl.diffuse.y, vpl.diffuse.z * pl.diffuse.z) / (1 + distance);
        //vpl.diffuse = pl.diffuse / (1 + distance);
        //std::cout << vpl.diffuse.x << ", " << vpl.diffuse.y << ", " << vpl.diffuse.z << std::endl;
        vpl.specular = pl.specular / (1 + distance);
        //vpl.specular = glm::vec3(0);
        vpls.push_back(vpl);
        
      }
    }

    clEnqueueWriteBuffer(clQueue, clVPLs, CL_TRUE, 0, noOfVPLS * sizeof(Light), vpls.data(), 0, NULL, NULL);

	std::vector<Light> vpls2;
	for(int i = 0; i < vpls.size(); ++i){
		Light vpl;
		vpl.position = pl.position;
		vpls2.push_back(vpl);
		vpls2.push_back(vpls[i]);		
	}

    glGenBuffers(1, &vpl_vbo);
    glGenVertexArrays(1, &vpl_vao);
    glBindVertexArray(vpl_vao);
    glBindBuffer(GL_ARRAY_BUFFER, vpl_vbo);
    glBufferData(GL_ARRAY_BUFFER, vpls2.size() * sizeof(Light), vpls2.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Light), (void*)offsetof(Light, position));
    glBindVertexArray(0);
      
  }

  glEnable(GL_DEPTH_TEST);
/*
  glViewport(0, 0, dpth_width, dpth_height);
  glBindFramebuffer(GL_FRAMEBUFFER, dpth_fbo);
  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glUseProgram(dpth_shader);
  //glUniformMatrix4fv(glGetUniformLocation(dpth_shader, "model"), 1, GL_FALSE, &model[0][0]);

  glUniformMatrix4fv(glGetUniformLocation(dpth_shader, "shadowMatrices[0]"), 1, GL_FALSE, &dpth_trnsfrms[0][0][0]);
  glUniformMatrix4fv(glGetUniformLocation(dpth_shader, "shadowMatrices[1]"), 1, GL_FALSE, &dpth_trnsfrms[1][0][0]);
  glUniformMatrix4fv(glGetUniformLocation(dpth_shader, "shadowMatrices[2]"), 1, GL_FALSE, &dpth_trnsfrms[2][0][0]);
  glUniformMatrix4fv(glGetUniformLocation(dpth_shader, "shadowMatrices[3]"), 1, GL_FALSE, &dpth_trnsfrms[3][0][0]);
  glUniformMatrix4fv(glGetUniformLocation(dpth_shader, "shadowMatrices[4]"), 1, GL_FALSE, &dpth_trnsfrms[4][0][0]);
  glUniformMatrix4fv(glGetUniformLocation(dpth_shader, "shadowMatrices[5]"), 1, GL_FALSE, &dpth_trnsfrms[5][0][0]);
  glUniform3fv(glGetUniformLocation(dpth_shader, "lightPos"), 1, &pl[0]);
  glUniform1f(glGetUniformLocation(dpth_shader, "far_plane"), dpth_far_plane);
  Model::draw(dpth_shader);
*/
  glViewport(0, 0, p_width, p_height);
  glm::mat4 view = Camera::getViewMatrix();
  glm::vec3 position = Camera::getPosition();
  glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(gBufferShader);
  //glUniformMatrix4fv(glGetUniformLocation(gBufferShader, "model"), 1, GL_FALSE, &model[0][0]);
  glUniformMatrix4fv(glGetUniformLocation(gBufferShader, "view"), 1, GL_FALSE, &view[0][0]);
  glUniformMatrix4fv(glGetUniformLocation(gBufferShader, "projection"), 1, GL_FALSE, &projection[0][0]);
  Model::draw(gBufferShader);

  size_t global_item_size[3] = {p_width, p_height, noOfVPLS};
  size_t local_item_size[3] = {1, 1, 1};
  glFinish();
  clEnqueueAcquireGLObjects(clQueue, 1, &clPositions, 0, 0, NULL);
  clEnqueueNDRangeKernel(clQueue, clPreRaysKernel, 3, NULL, global_item_size, local_item_size, 0, NULL, NULL);

  //clFinish(clQueue);

  //intersectionApi->QueryIntersection(rrRays, p_width * p_height * noOfVPLS, rrIsects, nullptr, nullptr);
  intersectionApi->QueryOcclusion(rrRays, p_width * p_height * noOfVPLS, rrOcclus, nullptr, nullptr);

  //clFinish(clQueue);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  clEnqueueNDRangeKernel(clQueue, clPostRaysKernel, 3, NULL, global_item_size, local_item_size, 0, NULL, NULL);
  clEnqueueReleaseGLObjects(clQueue, 1, &clMasks, 0, 0, NULL);
  clEnqueueReleaseGLObjects(clQueue, 1, &clPositions, 0, 0, NULL);
  clFinish(clQueue);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);
/*  glUseProgram(dPlaneShader);
  glUniform3fv(glGetUniformLocation(dPlaneShader, "light.position"), 1, &pl[0]);
  glUniform3fv(glGetUniformLocation(dPlaneShader, "light.diffuse"), 1, &pl_col[0]);
  glUniform3fv(glGetUniformLocation(dPlaneShader, "light.specular"), 1, &pl_col[0]);
  glUniform3fv(glGetUniformLocation(dPlaneShader, "viewPos"), 1, &position[0]);
  glUniform1f(glGetUniformLocation(dPlaneShader, "far_plane"), dpth_far_plane);
  glUniform1i(glGetUniformLocation(dPlaneShader, "gPosition"), 0);
  glUniform1i(glGetUniformLocation(dPlaneShader, "gNormal"), 1);
  glUniform1i(glGetUniformLocation(dPlaneShader, "gAlbedo"), 2);
  glUniform1i(glGetUniformLocation(dPlaneShader, "gSpecular"), 3);
  glUniform1i(glGetUniformLocation(dPlaneShader, "dMap"), 4);*/
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gPosition);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, gNormal);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, gAlbedo);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, gSpecular);
  //glActiveTexture(GL_TEXTURE4);
  //glBindTexture(GL_TEXTURE_CUBE_MAP, dpth_cbmp);
  glBindVertexArray(dPlaneVAO);
  //glDrawArrays(GL_TRIANGLES, 0, 6);

  glUseProgram(iPlaneShader);
  glUniform3fv(glGetUniformLocation(iPlaneShader, "viewPos"), 1, &position[0]);
  glUniform1i(glGetUniformLocation(iPlaneShader, "gPosition"), 0);
  glUniform1i(glGetUniformLocation(iPlaneShader, "gNormal"), 1);
  glUniform1i(glGetUniformLocation(iPlaneShader, "gAlbedo"), 2);
  glUniform1i(glGetUniformLocation(iPlaneShader, "gSpecular"), 3);
  for(int i = 0; i < noOfVPLS; ++i){
    glUniform3fv(glGetUniformLocation(iPlaneShader, ("vpls[" + std::to_string(i) + "].position").c_str()), 1, &vpls[i].position[0]);
    glUniform3fv(glGetUniformLocation(iPlaneShader, ("vpls[" + std::to_string(i) + "].diffuse").c_str()), 1, &vpls[i].diffuse[0]);
    glUniform3fv(glGetUniformLocation(iPlaneShader, ("vpls[" + std::to_string(i) + "].specular").c_str()), 1, &vpls[i].specular[0]);
  }
  glUniform1i(glGetUniformLocation(iPlaneShader, "vplMasks"), 4);
  glUniform1i(glGetUniformLocation(iPlaneShader, "noOfVPLs"), vpls.size());
  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_2D_ARRAY, vMasks);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  glUseProgram(vpl_shader);
  glUniformMatrix4fv(glGetUniformLocation(vpl_shader, "model"), 1, GL_FALSE, &Model::getModelMatrix()[0][0]);
  glUniformMatrix4fv(glGetUniformLocation(vpl_shader, "view"), 1, GL_FALSE, &view[0][0]);
  glUniformMatrix4fv(glGetUniformLocation(vpl_shader, "projection"), 1, GL_FALSE, &projection[0][0]);
  glBindVertexArray(vpl_vao);
  glDrawArrays(GL_LINES, 0, vpls.size() * 2);

}

void renderer::destroy(){

}
