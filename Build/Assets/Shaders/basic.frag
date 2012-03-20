#version 330

uniform vec3 lightPos;

out vec4 out_colour;
smooth in vec3 oNormal;
smooth in vec3 worldPos;

void main()
{	
	vec3 lightDir = normalize(lightPos - worldPos);
	float nDotL = max(0.0, dot(lightDir, -oNormal));
	vec3 lightColour = vec3(1.0, 1.0, 1.0) * nDotL;
	lightColour.x = min(lightColour.x + 0.1, 1.0);
	out_colour = vec4(lightColour, 1.0);
}

