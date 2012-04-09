#version 330

layout(location = 0)out vec3 out_colour;
smooth in vec3 oNormal;

void main()
{		
	out_colour = normalize(oNormal);
}

