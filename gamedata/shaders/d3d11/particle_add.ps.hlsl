#include "common.hlsli"

#ifdef USE_PARTICLE_AMBIENT_LIGHT
// Exponent on the saturated scene luminance (1 = linear, as in r2).
#define PARTICLE_AMBIENT_LIGHT_POWER 2.0f

float ParticleAmbientLight()
{
    float3 SceneLight = L_ambient.xyz + L_hemi_color.xyz + L_sun_color.xyz;
    float Light = saturate(dot(SceneLight, float3(0.2126f, 0.7152f, 0.0722f)));
    return pow(Light, PARTICLE_AMBIENT_LIGHT_POWER);
}
#endif

struct v2p
{
    float2 tc : TEXCOORD0;
    float4 c : COLOR0;

    float3 tctexgen : TEXCOORD1;
    float4 hpos : SV_POSITION;
    float fog : FOG;
};

void main(v2p I, out IXRayForward O)
{
    float4 result = I.c * s_base.Sample(smp_base, I.tc);

#if defined(USE_SOFT_PARTICLES) && !defined(DISABLE_SOFT_PARTICLES)
    float3 Point = GbufferGetPoint(I.hpos.xy);
    float spaceDepth = Point.z - I.tctexgen.z;
    result *= Contrast(saturate(spaceDepth * 1.3f), 2.0f);
#endif

    clip(result.w - EPS);

#ifdef USE_PARTICLE_AMBIENT_LIGHT
    // Mist and splashes drawn additively: follow the scene light, see particle.ps.hlsl.
    result.xyz *= ParticleAmbientLight();
#endif

    O.Color.xyz = GammaToLinear(result.xyz * I.fog);
    O.Color.w = result.w * I.fog;

#ifndef USE_PBR
	O.Color.w = GammaToLinear(O.Color.w);
#endif
	
	O.Velocity = 0.0f;
}

