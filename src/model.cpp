#include <iostream>
#include <string>
#include <cstring>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GL/glew.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "model.h"

namespace RR = RadeonRays;

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 texCoord;
};

struct Material {
  glm::vec3 ambient;
  glm::vec3 diffuse;
  glm::vec3 specular;
  glm::vec3 transmittance;
  float shininess;
  unsigned int diffuse_texture;
  unsigned int specular_texture;
  unsigned int bump_texture;
  unsigned int mask_texture;
  SDL_Surface* diffuse_surface;
  SDL_Surface* specular_surface;
  SDL_Surface* bump_surface;
  SDL_Surface* mask_surface;
};

struct Mesh {
  unsigned int vao;
  unsigned int vbo;
  unsigned int count;
  Material* material;
  RR::Shape* shape;
  std::vector<Vertex> vertices;
};

std::vector<Material> materials;
std::vector<Mesh> meshes;
glm::mat4 model;
glm::mat4 invPrevModel;

bool Model::init(INIReader config, RR::IntersectionApi* intersectionApi){
  std::string filename = config.Get("model", "filename", "INVALID");
  std::string path = config.Get("model", "path", "INVALID");

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> mats;
  std::string warn;
  std::string err;
  if(!tinyobj::LoadObj(&attrib, &shapes, &mats, &warn, &err, (path + filename).c_str(), path.c_str())){
    std::cerr << "Failed to load Model: " << err << std::endl;
    return false;
  }

  int imgFlags = IMG_INIT_JPG | IMG_INIT_PNG;
	if(!(IMG_Init(imgFlags) & imgFlags)){
		std::cerr << "Failed initialise SDL_Image: %s" << IMG_GetError() << std::endl;
		return false;
	}

  for(const auto& mat : mats){
    Material material;
    material.ambient = glm::vec3(
      mat.ambient[0],
      mat.ambient[1],
      mat.ambient[2]
    );
    material.diffuse = glm::vec3(
      mat.diffuse[0],
      mat.diffuse[1],
      mat.diffuse[2]
    );
    material.specular = glm::vec3(
      mat.specular[0],
      mat.specular[1],
      mat.specular[2]
    );
    material.transmittance = glm::vec3(
      mat.transmittance[0],
      mat.transmittance[1],
      mat.transmittance[2]
    );
    material.shininess = mat.shininess;

    material.diffuse_texture = 0;
    if(mat.diffuse_texname != ""){
      SDL_Surface* surface = IMG_Load((path + mat.diffuse_texname).c_str());
      if(!surface){
        std::cerr << "Failed to load diffuse texture " << path << mat.diffuse_texname << ": " << IMG_GetError() << std::endl;
      }

      glGenTextures(1, &material.diffuse_texture);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, material.diffuse_texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, surface->w, surface->h, 0, GL_RGB, GL_UNSIGNED_BYTE, surface->pixels);
	  glGenerateMipmap(GL_TEXTURE_2D);
	
  	  material.diffuse_surface = surface; 
    }

    material.specular_texture = 0;
    if(mat.specular_texname != ""){
      SDL_Surface* surface = IMG_Load((path + mat.specular_texname).c_str());
      if(!surface){
        std::cerr << "Failed to load specular texture " << path << mat.specular_texname << ": " << IMG_GetError() << std::endl;
      }

      glGenTextures(1, &material.specular_texture);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, material.specular_texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, surface->w, surface->h, 0, GL_RED, GL_UNSIGNED_BYTE, surface->pixels);
  		glGenerateMipmap(GL_TEXTURE_2D);
  		
  	  material.specular_surface = surface;
    }

    material.bump_texture = 0;
    if(mat.bump_texname != ""){
      SDL_Surface* surface = IMG_Load((path + mat.bump_texname).c_str());
      if(!surface){
        std::cerr << "Failed to load bump texture " << path << mat.bump_texname << ": " << IMG_GetError() << std::endl;
      }

      glGenTextures(1, &material.bump_texture);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, material.bump_texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, surface->w, surface->h, 0, GL_RED, GL_UNSIGNED_BYTE, surface->pixels);
  		glGenerateMipmap(GL_TEXTURE_2D);

  	  material.bump_surface = surface;
    }

    material.mask_texture = 0;
    if(mat.alpha_texname != ""){
      SDL_Surface* surface = IMG_Load((path + mat.alpha_texname).c_str());
      if(!surface){
        std::cerr << "Failed to load mask texture " << path << mat.alpha_texname << ": " << IMG_GetError() << std::endl;
      }

      glGenTextures(1, &material.mask_texture);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, material.mask_texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, surface->w, surface->h, 0, GL_RED, GL_UNSIGNED_BYTE, surface->pixels);
  		glGenerateMipmap(GL_TEXTURE_2D);

  		
  	  material.mask_surface = surface;
    }

    materials.push_back(material);
  }
  glBindTexture(GL_TEXTURE_2D, 0);

  float scale = config.GetReal("model", "scale", 1.f);
  model = glm::mat4(1.0f);
  model = glm::scale(model, glm::vec3(scale));
  invPrevModel = model;
  RR::matrix rModel = RR::scale(RR::float3(scale, scale, scale));
  RR::matrix rModelInv = RR::inverse(rModel);

  for(const auto& shape : shapes){
    Mesh mesh;

    for(const auto& index : shape.mesh.indices) {
      Vertex vertex;

      vertex.position = glm::vec3(
        attrib.vertices[3 * index.vertex_index + 0],
        attrib.vertices[3 * index.vertex_index + 1],
        attrib.vertices[3 * index.vertex_index + 2]
      );
      vertex.normal = glm::vec3(
        attrib.normals[3 * index.normal_index + 0],
        attrib.normals[3 * index.normal_index + 1],
        attrib.normals[3 * index.normal_index + 2]
      );
      vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);
      if(index.texcoord_index != -1)
        vertex.texCoord = glm::vec2(
          attrib.texcoords[2 * index.texcoord_index + 0],
          1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
        );
      mesh.vertices.push_back(vertex);
    }

    mesh.count = mesh.vertices.size();
    mesh.material = &(materials[shape.mesh.material_ids[0]]);

    glGenBuffers(1, &mesh.vbo);
    glGenVertexArrays(1, &mesh.vao);

    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);

    glBufferData(GL_ARRAY_BUFFER, mesh.count * sizeof(Vertex), mesh.vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));

    glBindVertexArray(0);

    int indices[mesh.vertices.size()];
    for(int i = 0; i < mesh.vertices.size(); ++i){
      indices[i] = i;
    }

    int numfaces = mesh.vertices.size()/3;
    int numfaceverts[numfaces];
    for(int i = 0; i < numfaces; ++i){
      numfaceverts[i] = 3;
    }

    mesh.shape = intersectionApi->CreateMesh((const float*)mesh.vertices.data(), mesh.count, sizeof(Vertex), indices, 0, numfaceverts, numfaces);
	mesh.shape->SetTransform(rModel, rModelInv);
	mesh.shape->SetId(meshes.size());
    intersectionApi->AttachShape(mesh.shape);

    meshes.push_back(mesh);
  }
  intersectionApi->Commit();

  return true;
}

void Model::draw(unsigned int shader){
  for(const auto& mesh : meshes){
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, &model[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "invPrevModel"), 1, GL_FALSE, &invPrevModel[0][0]);
    glUniform3fv(glGetUniformLocation(shader, "DiffuseColor"), 1, &(mesh.material->diffuse)[0]);
    glUniform3fv(glGetUniformLocation(shader, "SpecularColor"), 1, &(mesh.material->specular)[0]);
    glUniform1f(glGetUniformLocation(shader, "shininess"), mesh.material->shininess);
    glUniform1f(glGetUniformLocation(shader, "hasDiffuse"), mesh.material->diffuse_texture);
    glUniform1f(glGetUniformLocation(shader, "hasSpecular"), mesh.material->specular_texture);
    glUniform1f(glGetUniformLocation(shader, "hasBump"), mesh.material->bump_texture);
    glUniform1f(glGetUniformLocation(shader, "hasMask"), mesh.material->mask_texture);
    glUniform1i(glGetUniformLocation(shader, "DiffuseTexture"), 0);
    glUniform1i(glGetUniformLocation(shader, "SpecularTexture"), 1);
    glUniform1i(glGetUniformLocation(shader, "BumpTexture"), 2);
    glUniform1i(glGetUniformLocation(shader, "MaskTexture"), 3);
    glBindVertexArray(mesh.vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mesh.material->diffuse_texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mesh.material->specular_texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, mesh.material->bump_texture);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, mesh.material->mask_texture);
    glDrawArrays(GL_TRIANGLES, 0, mesh.count);
  }
  glBindVertexArray(0);
}

void Model::destroy(){
  IMG_Quit();
}

glm::mat4 Model::getModelMatrix(){
	return model;
}

//http://sdl.beuc.net/sdl.wiki/Pixel_Access
Uint32 getpixel(SDL_Surface *surface, int x, int y)
{
	if(x < 0) x = 0;
	if(y < 0) y = 0;
	
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to retrieve */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
    case 1:
        return *p;
        break;

    case 2:
        return *(Uint16 *)p;
        break;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
            return p[0] << 16 | p[1] << 8 | p[2];
        else
            return p[0] | p[1] << 8 | p[2] << 16;
        break;

    case 4:
        return *(Uint32 *)p;
        break;

    default:
        return 0;       /* shouldn't happen, but avoids warnings */
    }
}

glm::vec4 Model::getDiffuse(unsigned int mesh_id, unsigned int face_id, float x, float y){
	Mesh mesh = meshes[mesh_id];
	Vertex v0 = mesh.vertices[face_id * 3 + 0];
	Vertex v1 = mesh.vertices[face_id * 3 + 1];
	Vertex v2 = mesh.vertices[face_id * 3 + 2];
	glm::vec4 diffuse;
	if(mesh.material->diffuse_texture){
		int u, v;
		u = (1-x-y)*v0.texCoord.x + x*v1.texCoord.x + y*v2.texCoord.x;
		v = (1-x-y)*v0.texCoord.y + x*v1.texCoord.y + y*v2.texCoord.y;
		//Uint8* pixel = (Uint8*)mesh.material->diffuse_surface->pixels + v * mesh.material->diffuse_surface->pitch + u * 3;
		//glm::vec3 diffuse = glm::vec3(pixel[0]/255.f, pixel[1]/255.f, pixel[2]/255.f);
		Uint32 pixel = getpixel(mesh.material->diffuse_surface, u, v);
		Uint8 rgb[3];
		
		SDL_GetRGB(pixel, mesh.material->diffuse_surface->format, &rgb[0], &rgb[1], &rgb[2]);
		diffuse = glm::vec4(rgb[0], rgb[1], rgb[2], 255) / 255.f;
	}else{
		diffuse = glm::vec4(mesh.material->diffuse, 1);
	}
	return diffuse;
}

glm::vec4 Model::getSpecular(unsigned int mesh_id, unsigned int face_id, float x, float y){
	Mesh mesh = meshes[mesh_id];
	Vertex v0 = mesh.vertices[face_id * 3 + 0];
	Vertex v1 = mesh.vertices[face_id * 3 + 1];
	Vertex v2 = mesh.vertices[face_id * 3 + 2];
	glm::vec4 specular;
	if(mesh.material->specular_texture){
		int u, v;
		u = (1-x-y)*v0.texCoord.x + x*v1.texCoord.x + y*v2.texCoord.x;
		v = (1-x-y)*v0.texCoord.y + x*v1.texCoord.y + y*v2.texCoord.y;
		Uint32 pixel = getpixel(mesh.material->specular_surface, u, v);
		Uint8 rgb[3];
		
		SDL_GetRGB(pixel, mesh.material->specular_surface->format, &rgb[0], &rgb[1], &rgb[2]);
		specular = glm::vec4(rgb[0], rgb[1], rgb[2], 255) / 255.f;
	}else{
		specular = glm::vec4(mesh.material->specular, 1);
	}
	return specular;
}

glm::vec4 Model::getNormal(unsigned int mesh_id, unsigned int face_id, float x, float y){
	Mesh mesh = meshes[mesh_id];
	Vertex v0 = mesh.vertices[face_id * 3 + 0];
	Vertex v1 = mesh.vertices[face_id * 3 + 1];
	Vertex v2 = mesh.vertices[face_id * 3 + 2];
	glm::vec4 normal;
	normal.x = (1-x-y)*v0.normal.x + x*v1.normal.x + y*v2.normal.x;
	normal.y = (1-x-y)*v0.normal.y + x*v1.normal.y + y*v2.normal.y;
	normal.z = (1-x-y)*v0.normal.z + x*v1.normal.z + y*v2.normal.z;
	normal.w = 0;
	return normal;
}