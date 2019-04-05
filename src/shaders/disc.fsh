#version 330 core
out vec3 FragColor;

in vec2 TexCoords;

uniform sampler2D gNormal;
uniform sampler2D gIndirect;
uniform sampler2D gPosition;

uniform int pass;
uniform int iss;
uniform float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
uniform float cutoff = 20;

uniform float idScale;

void main(){
	vec3 normal = texture(gNormal, TexCoords * idScale).xyz;
	vec3 indirect = texture(gIndirect, TexCoords * idScale).rgb * weight[0];
	vec2 tex_offset = 1.0 / textureSize(gNormal, 0);
	if(pass == 1){
		for(int x = 1; x < 5; ++x){
			vec2 offsetX = vec2(tex_offset.x * x, 0);
			vec3 normalXP = texture(gNormal, (TexCoords + offsetX) * idScale).xyz;
			if(distance(normalXP, normal) < cutoff)
				indirect += texture(gIndirect, (TexCoords + offsetX) * idScale).rgb * weight[x];
			vec3 normalXM = texture(gNormal, (TexCoords - offsetX) * idScale).xyz;
			if(distance(normalXM, normal) < cutoff)
				indirect += texture(gIndirect, (TexCoords - offsetX) * idScale).rgb * weight[x];
		}
	}else{
		for(int y = 1; y < 5; ++y){
			vec2 offsetY = vec2(0, tex_offset.y * y);
			vec3 normalYP = texture(gNormal, (TexCoords + offsetY) * idScale).xyz;
			if(distance(normalYP, normal) < cutoff)
				indirect += texture(gIndirect, (TexCoords + offsetY) * idScale).rgb * weight[y];
			vec3 normalYM = texture(gNormal, (TexCoords + offsetY) * idScale).xyz;
			if(distance(normalYM, normal) < cutoff)
				indirect += texture(gIndirect, (TexCoords - offsetY) * idScale).rgb * weight[y];
		}
	}
	
	FragColor = indirect * iss;
}
