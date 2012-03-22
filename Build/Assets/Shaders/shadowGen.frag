#version 330

out vec2 out_colour;
smooth in float depth;
uniform float lightRadius;

void main()
{		
	out_colour = vec2(depth, pow(depth, 2.0));
}

