#version 330 core
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec3 gAlbedo;
layout (location = 3) out float gSpecular;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform float hasDiffuse;
uniform float hasSpecular;
uniform float hasBump;
uniform float hasMask;

uniform sampler2D DiffuseTexture;
uniform sampler2D SpecularTexture;
uniform sampler2D BumpTexture;
uniform sampler2D MaskTexture;

uniform vec3 DiffuseColor;
uniform vec3 SpecularColor;

void main(){
  if(hasMask != 0 && texture(MaskTexture, TexCoord).r == 0)
    discard;
  gPosition = FragPos;
  gNormal = Normal;
  if(hasDiffuse != 0)
    gAlbedo = texture(DiffuseTexture, TexCoord).rgb;
  else
    gAlbedo = DiffuseColor;
  if(hasSpecular != 0)
    gSpecular = texture(SpecularTexture, TexCoord).r;
  else
    gSpecular = 0;
}
