#version 330

layout(location = 0) in vec3 position;
uniform mat4 lightWorldView;
uniform mat4 projection;
smooth out vec3 worldPos;
smooth out float depth;
uniform float lightRadius;

void main()
{	
	vec4 lightViewPos = lightWorldView * vec4(position, 1.0);	
	float dist = length(lightViewPos.xyz);
	lightViewPos /= dist;
	lightViewPos.z += 1;
	lightViewPos.xy /= lightViewPos.z;
	lightViewPos.z = dist / lightRadius;
	depth = lightViewPos.z;
	lightViewPos.w = 1;
	gl_Position = lightViewPos;		
} 
