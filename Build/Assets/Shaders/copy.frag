#version 330

out vec4 out_colour;
in vec2 fragTexCoord;

uniform vec2 pixSize;
uniform sampler2D baseTex;

void main()
{	
	out_colour = vec4(1.0, 0.0, 0.0, 1.0);
}

