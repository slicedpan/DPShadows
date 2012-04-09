#version 330

smooth in vec2 fragTexCoord;

layout(location = 0)out float out_colour;

uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D noiseTex;

uniform mat4 invViewProj;
const vec2 pixSize = vec2(0.00125, 0.00166666);

const vec3 samplePoints[4] = { vec3(1, 1, 1), vec3(-1, -1, 1), vec3(-1, 1,-1), vec3(1, -1, -1) };

float computeAO(vec2 baseCoord, vec3 samplePoint, float baseDepth)
{
	samplePoint *= 6.0;
	samplePoint = reflect(samplePoint, texture(gNormal, fragTexCoord ).xyz);
	samplePoint.xy *= pixSize;
	samplePoint.z /= 100000.0;
	float sampleDepth = texture(gDepth, fragTexCoord + samplePoint.xy).x;
	//return abs(sampleDepth  + samplePoint.z - baseDepth) * 1000.0;
	if (sampleDepth + samplePoint.z > baseDepth)
		return 1.0;
	else 
		return 0.0;
}

void main()
{		
	float baseDepth = texture(gDepth, fragTexCoord).x;
	out_colour = 0.0;
	for (int i = 0; i < 4; ++i)
	{	
		out_colour += computeAO(fragTexCoord, samplePoints[i], baseDepth);		
	}
	out_colour /= 4.0;
} 