#version 330

layout(location = 0) in vec3 position;
uniform mat4 lightWorldView;
uniform mat4 projection;
smooth out vec3 worldPos;
smooth out float depth;
uniform float lightRadius;

void main()
{	
	vec4 lightViewPos = projection * lightWorldView * vec4(position, 1.0);
	depth = lightViewPos.z / lightRadius;
	gl_Position = lightViewPos;		
} 
