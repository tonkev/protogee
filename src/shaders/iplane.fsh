#version 330 core
layout (location = 0) out vec3 gIndirect;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;

struct Light {
  vec3 position;
  vec3 diffuse;
  vec3 specular;
};
#define MAX_NO_OF_LIGHTS 16
#define MAX_NO_OF_VPLS 512
uniform Light pls[MAX_NO_OF_LIGHTS];
uniform Light vpls[MAX_NO_OF_VPLS];
uniform sampler2DArray vplMasks;

uniform vec3 viewPos;
uniform int noOfLights;
uniform int noOfVPLs;

uniform int iHistoryIndex;
uniform int iHistorySize;

uniform int noOfVPLBounces = 1;

uniform int debugVPLI = -1;

void main(){
  vec3 norm = normalize(texture(gNormal, TexCoords).xyz);
  vec3 fragPos = texture(gPosition, TexCoords).xyz;

  vec3 diffuse = vec3(0);

  const float PI = 3.14159;

  for(int j = 0; j < noOfVPLs/iHistorySize; ++j){
	int i = (j * iHistorySize) + iHistoryIndex;
  	if(debugVPLI == -1 || debugVPLI == i){
	    float visibility = texture(vplMasks, vec3(TexCoords, i)).r;
	    float dist = distance(vpls[i].position, fragPos);
			int firstBounceVPLI = int(mod(i, int(noOfVPLs / noOfVPLBounces)));
			Light pl = pls[firstBounceVPLI % noOfLights];
	    dist += distance(pl.position, vpls[firstBounceVPLI].position);
	    float attenuation = 1 / (1 + dist * dist);
	
	    vec3 lightDir = normalize(vpls[i].position - fragPos);
	    float diff = max(dot(norm, lightDir), 0);
	    diffuse += diff * vpls[i].diffuse * visibility * attenuation / PI;
			//diffuse += vec3(visibility);
	  }
  }

  gIndirect = diffuse;
}
