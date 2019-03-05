#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec3 Color;
in vec2 TexCoord;

uniform sampler2D Texture;

void main(){
  FragColor = texture(Texture, TexCoord);
}
