#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
uniform mat4 lightWorldView;
uniform mat4 projection;
smooth out vec3 worldPos;
smooth out float depth;
uniform float lightRadius;
uniform vec3 lightPosition;

void main()
{	
	vec4 lightViewPos = lightWorldView * vec4(position, 1.0);
	vec3 lightDir = lightPosition - position;
	lightDir = normalize(lightDir);
	orthogonality = max(dot(lightDir, normal);
	float dist = length(lightViewPos.xyz);
	lightViewPos /= dist;
	lightViewPos.z += 1;
	lightViewPos.xy /= lightViewPos.z;
	lightViewPos.z = dist / lightRadius;
	depth = lightViewPos.z;
	lightViewPos.w = 1;
	gl_Position = lightViewPos;		
} 
