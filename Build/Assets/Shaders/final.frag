#version 330

out vec4 out_colour;
smooth in vec2 fragTexCoord;

uniform vec2 pixSize;
uniform sampler2D baseTex;
uniform sampler2D gDepth;
uniform sampler2D occlusionBuf;
uniform sampler2D halfTex;
uniform sampler2D noiseTex;
uniform float currentDepth;

uniform int SSAO;
uniform int DOF;
uniform float maxDarkening = 0.6;

void main()
{	
	out_colour = texture(baseTex, fragTexCoord);
	if (DOF > 0)
	{
		vec4 blurredColour = texture(halfTex, fragTexCoord);
		out_colour = mix(out_colour, blurredColour, min(pow(abs(texture(gDepth, fragTexCoord).x - currentDepth), 1.0) * 80.0, 1.0));
	}
	if (SSAO > 0)
		out_colour*= (1 - texture(occlusionBuf, fragTexCoord).x);
}

