#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gAlbedo;

uniform sampler2D dColor;

uniform int iHistorySize;
uniform int iHistoryIndex;
uniform sampler2DArray iHistory;

uniform mat4 vHistory[16];
uniform sampler2DArray pHistory;

uniform mat4 projection;

uniform int iEnabled;
uniform int dEnabled;

void main(){
	vec3 col = vec3(0);
	if(iEnabled){
		col += texture(iHistory, vec3(TexCoords, iHistoryIndex)).rgb;
		vec3 cpos = texture(pHistory, vec3(TexCoords, iHistoryIndex)).xyz;
		for(int i = 1; i < iHistorySize; ++i){
			int ihi = (iHistoryIndex + i) % iHistorySize;
			vec4 clipSpace = projection * vHistory[ihi] * vec4(cpos, 1);
			vec3 ndc = clipSpace.xyz / clipSpace.w;
			vec2 screenSpace = (ndc.xy * .5 + .5);
			
			vec3 coords = vec3(screenSpace, ihi);
			vec3 pos = texture(pHistory, coords).xyz;
			if(distance(pos, cpos) < 0.1)
				col += texture(iHistory, coords).rgb;
		}
	}
	if(dEnabled)
		col += texture(dColor, TexCoords).rgb;
  col *= texture(gAlbedo, TexCoords).rgb;
  col = col / (col + vec3(1));	
	FragColor = vec4(col, 1);
}
