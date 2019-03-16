#version 330 core
out vec4 FragColor;

in float invalid;

void main(){
  if(invalid == 1)
  	discard;
  else
  	FragColor = vec4(0.0, 0.0, 1.0, 1.0);
}
