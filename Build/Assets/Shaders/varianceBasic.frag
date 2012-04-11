#version 330

uniform vec3 lightPos;
uniform float lightRadius;
uniform float invScreenHeight;
uniform float invScreenWidth;
uniform float noiseTexSize;

layout(location = 0)out vec4 out_colour;
smooth in vec3 oNormal;
smooth in vec3 worldPos;

uniform vec3 lightCone;
uniform mat4x4 lightWorldView;
uniform sampler2D shadowTex;
uniform sampler2D noiseTex;
uniform sampler2D gDepth;

uniform int samples = 4;
uniform float noiseMult = 0.6;

float getShadowAtten(vec2 texCoord, float baseDepth, float depthBias)
{
	float atten = 1.0;
	vec2 shadow = texture(shadowTex, texCoord).xy;
	if (shadow.x < baseDepth - depthBias)
	{
		atten = min(abs(pow(shadow.x, 2.0) - shadow.y) * 10.0, 1.0);
	}
	return atten;
}

void main()
{	
	vec2 screenTexCoords = gl_FragCoord.xy * vec2(invScreenWidth, invScreenHeight);
	float storedDepth =  texture(gDepth, screenTexCoords).x;
	
	if (abs(gl_FragCoord.z - storedDepth) > 0.000001)
		discard;
	vec3 lightDir = lightPos - worldPos;
	float lightDist = length(lightDir);
	lightDir /= lightDist;
	vec3 lightColour = vec3(0.0, 0.0, 0.0);
	float coneAtten = sqrt(max(dot(lightCone, lightDir), 0.0));
	vec2 noise[4];
	vec3 lightWVPos = (lightWorldView * vec4(worldPos, 1.0)).xyz;
	float nDotL = max(0.0, dot(lightDir, normalize(oNormal)));
	
	vec2 noiseCoord[4];
	noiseCoord[0] = lightWVPos.xy / 2.0 + vec2(0.5, 0.5);
	noiseCoord[0] *= 0.1;
	//noiseCoord[0] = gl_FragCoord.xy / 2.0 + vec2(0.5, 0.5);
	//noiseCoord[0] *= 0.25;
	noiseCoord[1] = noiseCoord[0] + vec2(0.0, invScreenHeight);
	noiseCoord[2] = noiseCoord[0] + vec2(invScreenWidth, -invScreenHeight);
	noiseCoord[3] = noiseCoord[0] + vec2(-invScreenWidth, invScreenHeight);
	
	for (int i = 0; i < samples; ++i)
	{
		noiseCoord[i].xy += 1.0;
		noiseCoord[i].xy /= 2.0;
		noiseCoord[i].x /= invScreenWidth;
		noiseCoord[i].y /= invScreenHeight;
		noiseCoord[i].xy /= noiseTexSize;	
		noiseCoord[i] *= 2.0;
	
		noise[i] = (texture(noiseTex, noiseCoord[i]).xy - vec2(0.5, 0.5)) * noiseMult;	

		noise[i].x *= invScreenWidth;
		noise[i].y *= invScreenHeight;
	}	
	
	lightWVPos = normalize(lightWVPos);
	lightWVPos.z += 1;
	lightWVPos.xy /= lightWVPos.z;
	lightWVPos.xy += 1.0;
	lightWVPos.xy /= 2.0;

	float realDepth = lightDist / lightRadius;
	
	float shadowAtten = 0.0;
	for (int i = 0; i < samples; ++i)
	{
		shadowAtten += getShadowAtten(lightWVPos.xy + noise[i], realDepth, -(lightRadius / 10000.0) * (1 - nDotL));		
	}
	shadowAtten /= samples;
	shadowAtten += 0.1;

	float attenuation = pow(max((1 - lightDist/lightRadius), 0.0), 2.0) * coneAtten * shadowAtten;
	lightColour = vec3(1.0, 1.0, 1.0) * nDotL * attenuation;
	lightColour.x = min(lightColour.x + 0.1, 1.0);
	
	out_colour = vec4(lightColour, 1.0);
}

