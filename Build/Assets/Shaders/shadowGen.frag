#version 330

uniform vec3 lightPos;

out vec2 out_colour;
smooth in float depth;
smooth in float depthSquared;

void main()
{		
	out_colour = vec2(depth, pow(depth, 2.0));
}

