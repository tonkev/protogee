#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gNormal;
uniform sampler2D gDirect;
uniform sampler2D gIndirect;

uniform int pass;
uniform float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
uniform float cutoff = 0.1;
uniform int directEnabled;
uniform int indirectEnabled;

void main(){
	vec3 normal = texture(gNormal, TexCoords).xyz;
	vec3 indirect = texture(gIndirect, TexCoords).rgb * weight[0];
	vec2 tex_offset = 1.0 / textureSize(gIndirect, 0);
	if(pass == 1){
		for(int x = 1; x < 5; ++x){
			vec2 offsetX = vec2(tex_offset.x * x, 0);
			vec3 normalXP = texture(gNormal, TexCoords + offsetX).xyz;
			if(distance(normalXP, normal) < cutoff)
				indirect += texture(gIndirect, TexCoords + offsetX).rgb * weight[x];
			vec3 normalXM = texture(gNormal, TexCoords - offsetX).xyz;
			if(distance(normalXM, normal) < cutoff)
				indirect += texture(gIndirect, TexCoords - offsetX).rgb * weight[x];
		FragColor = vec4(indirect, 1);
		}
	}else{
		vec3 col = vec3(0);
		if(indirectEnabled){
			for(int y = 1; y < 5; ++y){
				vec2 offsetY = vec2(0, tex_offset.y * y);
				vec3 normalYP = texture(gNormal, TexCoords + offsetY).xyz;
				if(distance(normalYP, normal) < cutoff)
					indirect += texture(gIndirect, TexCoords + offsetY).rgb * weight[y];
				vec3 normalYM = texture(gNormal, TexCoords - offsetY).xyz;
				if(distance(normalYM, normal) < cutoff)
					indirect += texture(gIndirect, TexCoords - offsetY).rgb * weight[y];
			}
			col += indirect;
			
		}
		if(directEnabled){
			vec3 direct = texture(gDirect, TexCoords).rgb;
			col += direct;
		}
		
		FragColor = vec4(col, 1);
	}
	
}