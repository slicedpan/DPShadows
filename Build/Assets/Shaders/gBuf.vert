#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

uniform mat4 projViewWorld;

smooth out vec3 oNormal;

void main()
{	
	gl_Position = projViewWorld * vec4(position, 1.0);	
	oNormal = normal;	
} 
