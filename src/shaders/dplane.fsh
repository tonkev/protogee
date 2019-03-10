#version 330 core
layout (location = 0) out vec3 gDirect;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D gSpecular;

uniform samplerCube dMap;

struct Light {
  vec3 position;
  vec3 diffuse;
  vec3 specular;
};
uniform Light light;

uniform vec3 viewPos;

uniform float far_plane;

void main(){
  vec3 norm = normalize(texture(gNormal, TexCoords).xyz);
  vec3 fragPos = texture(gPosition, TexCoords).xyz;
  float shininess = texture(gSpecular, TexCoords).r;
  vec3 viewDir = normalize(viewPos - fragPos);

  vec3 lightDir = normalize(light.position - fragPos);
  float diff = max(dot(norm, lightDir), 0.0);
  vec3 diffuse = diff * light.diffuse;

  vec3 reflectDir = reflect(-lightDir, norm);
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
  vec3 specular = shininess * spec * light.diffuse;

  vec3 fragToLight = fragPos - light.position;
  float closestDepth = texture(dMap, fragToLight).r;
  closestDepth *= far_plane;
  float currentDepth = length(fragToLight);
  float bias = 0.05;
  float shadow = currentDepth - bias > closestDepth ? 0.0 : 1.0;

  float dist = distance(light.position, fragPos);
  float attenuation = 1 / (1 + dist * dist);

  gDirect = (diffuse + specular) * shadow * attenuation * texture(gAlbedo, TexCoords).xyz;
}
