#version 330 core
layout (location = 0) out vec3 gDirect;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gSpecular;
uniform sampler2D dColor;

uniform samplerCube dCMap;
uniform sampler2D dMap;
uniform mat4 dProjection;

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
	
	float shadow = 1;
	vec3 fragToLight = fragPos - light.position;
	float closestDepth = texture(dCMap, fragToLight).r;
	closestDepth *= far_plane;
	float currentDepth = length(fragToLight);
	float bias = 0.1;
	shadow = currentDepth - bias > closestDepth ? 0.0 : 1.0;
	
	if(lightExtra.type == 1){
		float angle = dot(lightDir, -light.normal);
		if(angle < lightExtra.angle) shadow = 0;
	}
	if(lightExtra.type == 2){
		float hyp = distance(light.position, fragPos);
		vec3 dir = normalize(fragPos - light.position);
		float angle = acos(dot(dir, light.normal));
		float hypAngle = acos(dot(vec3(0, -1, 0), light.normal));
		float dist;
		if(hypAngle != 0)
			dist = sin(angle) / sin(hypAngle) * hyp;
		else
			dist = cos(angle) * hyp;
		vec3 samplePos = fragPos - (light.normal * dist);
		vec2 drift = abs(samplePos.xz - light.position.xz);
		if(drift.x > lightExtra.quad.x || drift.y > lightExtra.quad.y || fragPos.y > light.position.y) shadow = 0;
		else
			shadow = 1;
	}

	float dist = distance(light.position, fragPos);
	float attenuation = 1 / (1 + dist * dist);
	
	direct += (diffuse + specular) * shadow * attenuation;
	
  gDirect = direct;
}
