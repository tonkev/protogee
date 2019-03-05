#include <iostream>
#include <string>
#include <cstring>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
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
};

struct Mesh {
  unsigned int vao;
  unsigned int vbo;
  unsigned int count;
  Material* material;
  RR::Shape* shape;
};

std::vector<Material> materials;
std::vector<Mesh> meshes;

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
    }

    materials.push_back(material);
  }
  glBindTexture(GL_TEXTURE_2D, 0);

  for(const auto& shape : shapes){
    Mesh mesh;
    std::vector<Vertex> vertices;

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
      vertices.push_back(vertex);
    }

    mesh.count = vertices.size();
    mesh.material = &(materials[shape.mesh.material_ids[0]]);

    glGenBuffers(1, &mesh.vbo);
    glGenVertexArrays(1, &mesh.vao);

    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);

    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));

    glBindVertexArray(0);

    int indices[vertices.size()];
    for(int i = 0; i < vertices.size(); ++i){
      indices[i] = i;
    }

    int numfaces = vertices.size()/3;
    int numfaceverts[numfaces];
    for(int i = 0; i < numfaces; ++i){
      numfaceverts[i] = 3;
    }

    mesh.shape = intersectionApi->CreateMesh((const float*)vertices.data(), mesh.count, sizeof(Vertex), indices, 0, numfaceverts, numfaces);
    intersectionApi->AttachShape(mesh.shape);

    meshes.push_back(mesh);
  }
  intersectionApi->Commit();

  return true;
}

void Model::draw(unsigned int shader){
  for(const auto& mesh : meshes){
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
