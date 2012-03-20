#version 330

uniform vec3 lightPos;
uniform float lightRadius;

out vec4 out_colour;
smooth in vec3 oNormal;
smooth in vec3 worldPos;

void main()
{	
	vec3 lightDir = lightPos - worldPos;
	float lightDist = length(lightDir);
	lightDir /= lightDist;
	float nDotL = max(0.0, dot(lightDir, normalize(oNormal)));
	float attenuation = pow(max((1 - lightDist/lightRadius), 0.0), 2.0);
	vec3 lightColour = vec3(1.0, 1.0, 1.0) * nDotL * max((1 - lightDist/lightRadius), 0.0);
	lightColour.x = min(lightColour.x + 0.1, 1.0);
	out_colour = vec4(lightColour, 1.0);
}

