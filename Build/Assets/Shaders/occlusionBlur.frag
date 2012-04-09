#version 330

layout(location = 0) out float out_colour;

smooth in vec2 fragTexCoord;

uniform bool vertical;
uniform sampler2D baseTex;
const vec2 pixSize = vec2(0.00125, 0.00166666);
float weights[7] = { 0.00038771, 0.01330373,	0.11098164,	0.22508352, 	0.11098164 ,	0.01330373, 	0.00038771 };

void main()
{
	vec2 offset = vec2(1, 0);
	if (vertical)
		offset = vec2(0, 1);
	out_colour = 0.0;
	for (int i = 0; i < 7; ++i)
	{
		vec2 coord = (i - 3) * offset;
		out_colour += texture(baseTex, fragTexCoord + coord * pixSize).x * weights[i] * 2.10767;
	}	
}