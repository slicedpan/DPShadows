#version 330

out vec2 out_colour;
smooth in float depth;
smooth in float orthogonality;
uniform float lightRadius;

void main()
{		
	out_colour = vec2(depth, orthogonality);
}

