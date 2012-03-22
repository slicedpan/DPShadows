#version 330

uniform vec3 lightPos;
uniform float lightRadius;

layout(location = 0)out vec4 out_colour;
smooth in vec3 oNormal;
smooth in vec3 worldPos;

uniform vec3 lightCone;
uniform mat4x4 lightWorldView;
uniform sampler2D shadowTex;

float getShadowAtten(vec2 texCoord, float baseDepth)
{
	float atten = 1.0;
	vec2 shadow = texture(shadowTex, texCoord).xy;
	if (shadow.x < baseDepth + 0.00001)
	{
		atten = min(abs(pow(shadow.x, 2.0) - shadow.y) * 15.0, 1.0);
	}
	return atten;
}

void main()
{	
	vec3 lightDir = lightPos - worldPos;
	float lightDist = length(lightDir);
	lightDir /= lightDist;
	vec3 lightColour = vec3(0.0, 0.0, 0.0);
	float coneAtten = sqrt(max(dot(lightCone, lightDir), 0.0));
	
	vec3 lightWVPos = (lightWorldView * vec4(worldPos, 1.0)).xyz;
	lightWVPos = normalize(lightWVPos);
	lightWVPos.z += 1;
	lightWVPos.xy /= lightWVPos.z;
	lightWVPos.xy += 1.0;
	lightWVPos.xy /= 2.0;
	vec2 shadowInfo = texture(shadowTex, lightWVPos.xy).xy;
	float realDepth = lightDist / lightRadius;
	
	float shadowAtten = getShadowAtten(lightWVPos.xy, realDepth);	
	float offsetx = 1.0 / 800.0;
	float offsety = 1.0 / 600.0;
	shadowAtten += getShadowAtten(lightWVPos.xy + vec2(offsetx, offsety), realDepth);
	shadowAtten += getShadowAtten(lightWVPos.xy - vec2(offsetx, offsety), realDepth);
	shadowAtten /= 3.0;
	
	float nDotL = max(0.0, dot(lightDir, normalize(oNormal)));
	float attenuation = pow(max((1 - lightDist/lightRadius), 0.0), 2.0) * coneAtten * shadowAtten;
	lightColour = vec3(1.0, 1.0, 1.0) * nDotL * attenuation;
	lightColour.x = min(lightColour.x + 0.1, 1.0);
	
	out_colour = vec4(lightColour, 1.0);
}

