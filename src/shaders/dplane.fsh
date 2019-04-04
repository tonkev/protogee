#version 330 core
layout (location = 0) out vec3 gDirect;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gSpecular;
uniform sampler2D dColor;

uniform samplerCube dMap;

struct Light {
  vec3 position;
  vec3 diffuse;
  vec3 specular;
	vec3 normal;
};
uniform Light light;

struct LightExtra{
	int type;
	vec2 quad;
	float angle;
};
uniform LightExtra lightExtra;

uniform vec3 viewPos;

uniform float far_plane;

uniform int pass;

const float PI = 3.14159;

void main(){
  vec3 norm = normalize(texture(gNormal, TexCoords).xyz);
  vec3 fragPos = texture(gPosition, TexCoords).xyz;
  float shininess = texture(gSpecular, TexCoords).r;
  vec3 viewDir = normalize(viewPos - fragPos);
	vec3 direct = vec3(0);

	if(pass != 0){
		direct += texture(dColor, TexCoords).rgb;
	}
	
	vec3 lightDir = normalize(light.position - fragPos);
	float diff = max(dot(norm, lightDir), 0);
	vec3 diffuse = diff * light.diffuse / PI;

	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
	vec3 specular = shininess * spec * light.specular;
	
	vec3 fragToLight = fragPos - light.position;
	float closestDepth = texture(dMap, fragToLight).r;
	closestDepth *= far_plane;
	float currentDepth = length(fragToLight);
	float bias = 0.1;
	float shadow = currentDepth - bias > closestDepth ? 0.0 : 1.0;
	
	if(lightExtra.type == 1){
		float angle = dot(lightDir, -light.normal);
		if(angle < lightExtra.angle) shadow = 0;
	}

	float dist = distance(light.position, fragPos);
	float attenuation = 1 / (1 + dist * dist);
	
	direct += (diffuse + specular) * shadow * attenuation;
	
  gDirect = direct;
}
