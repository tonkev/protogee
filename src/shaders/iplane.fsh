#version 330 core
layout (location = 0) out vec3 gIndirect;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gSpecular;

struct Light {
  vec3 position;
  vec3 diffuse;
  vec3 specular;
};
#define MAX_NO_OF_VPLS 128
uniform Light pl;
uniform Light vpls[MAX_NO_OF_VPLS];
uniform sampler2DArray vplMasks;

uniform vec3 viewPos;
uniform int noOfVPLs;

uniform int iHistoryIndex;
uniform int iHistorySize;

uniform int debugVPLI = -1;

void main(){
  vec3 norm = normalize(texture(gNormal, TexCoords).xyz);
  vec3 fragPos = texture(gPosition, TexCoords).xyz;
  float shininess = texture(gSpecular, TexCoords).r;
  vec3 viewDir = normalize(viewPos - fragPos);

  vec3 diffuse = vec3(0);
  vec3 specular = vec3(0);

  for(int j = 0; j < noOfVPLs/iHistorySize; ++j){
	int i = (j * iHistorySize) + iHistoryIndex;
  	if(debugVPLI == -1 || debugVPLI == i){
	    float visibility = texture(vplMasks, vec3(TexCoords, i)).r;
	    float dist = distance(vpls[i].position, fragPos);
	    dist += distance(pl.position, vpls[i].position);
	    float attenuation = 1 / (1 + dist * dist);
	
	    vec3 lightDir = normalize(vpls[i].position - fragPos);
	    float diff = max(dot(norm, lightDir), 0.0);
	    diffuse += diff * vpls[i].diffuse * pl.diffuse * visibility * attenuation;
	    //diffuse += vec3(visibility);
	
	    vec3 reflectDir = reflect(-lightDir, norm);
	    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
	    //specular += shininess * spec * vpls[i].specular * pl.specular * visibility * attenuation;
	  }
  }

  gIndirect = min(diffuse + specular, 1) / noOfVPLs;
}
