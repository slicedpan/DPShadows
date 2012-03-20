#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

uniform mat4 View;
uniform mat4 Projection;
uniform mat4 World;

uniform vec3 lightPos;

out vec3 oNormal;
out vec3 worldPos;

void main()
{	
	worldPos = position;
	gl_Position = Projection * View * World * vec4(position, 1.0);
	oNormal = normalize(normal);	
} 
