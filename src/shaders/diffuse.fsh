#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec3 Color;
in vec2 TexCoord;

uniform sampler2D Texture;
uniform samplerCube depthMap;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 diffuseColor;

uniform float far_plane;

void main(){
  vec3 norm = normalize(Normal);
  vec3 lightDir = normalize(lightPos - FragPos);
  float diff = max(dot(norm, lightDir), 0.0);
  float attenuation = 10/length(lightPos - FragPos);
  vec3 diffuse = diff * lightColor;

  vec3 fragToLight = FragPos - lightPos;
  float closestDepth = texture(depthMap, fragToLight).r;
  closestDepth *= far_plane;
  float currentDepth = length(fragToLight);
  float bias = 0.35;
  float shadow = currentDepth - bias > closestDepth ? 0.0 : 1.0;

  FragColor = vec4(shadow * diffuse * texture(Texture, TexCoord).xyz, 1);
  //FragColor = vec4(vec3((closestDepth / far_plane)), 1.0);
  //FragColor = vec4(diffuse, 1);
}
