#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D gSpecular;

struct Light {
  vec3 position;
  vec3 diffuse;
  vec3 specular;
};
#define MAX_NO_OF_VPLS 128
uniform Light vpls[MAX_NO_OF_VPLS];
uniform sampler2DArray vplMasks;

uniform vec3 viewPos;
uniform int noOfVPLs;

void main(){
  vec3 norm = normalize(texture(gNormal, TexCoords).xyz);
  vec3 fragPos = texture(gPosition, TexCoords).xyz;
  float shininess = texture(gSpecular, TexCoords).r;
  vec3 viewDir = normalize(viewPos - fragPos);

  vec3 diffuse = vec3(0);
  vec3 specular = vec3(0);

  for(int i = 0; i < noOfVPLs; ++i){
  //int i = 3;{
    float visibility = texture(vplMasks, vec3(TexCoords, i)).r;
    float attenuation = 1 / (1 + distance(vpls[i].position, fragPos));

    vec3 lightDir = normalize(vpls[i].position - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    diffuse += diff * vpls[i].diffuse * visibility * attenuation;

    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    //specular += shininess * spec * vpls[i].specular * visibility * attenuation;
  	
  }

  FragColor = vec4(min(diffuse + specular, 1) * texture(gAlbedo, TexCoords).xyz, 1);
  //FragColor = vec4(texture(vplMasks, vec3(TexCoords, 0)).r, 0, 0.2, 1);
}
