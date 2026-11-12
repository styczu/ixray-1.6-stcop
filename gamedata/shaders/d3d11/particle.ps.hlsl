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

// Pixel
void main(v2p I, out IXRayForward O)
{
    float4 result = I.c * s_base.Sample(smp_base, I.tc);

#if defined(USE_SOFT_PARTICLES) && !defined(DISABLE_SOFT_PARTICLES)
    float3 Point = GbufferGetPoint(I.hpos.xy);
    float spaceDepth = Point.z - I.tctexgen.z;
    result *= Contrast(saturate(spaceDepth * 1.3f), 2.0f);
#endif

   // clip(result.a - (0.01f / 255.0f));
	result = lerp(fog_color, result, I.fog);

#ifdef USE_PARTICLE_AMBIENT_LIGHT
    // Translucent particles (smoke, splashes, dust) have no lighting of their own. Scale by the
    // scene light so they do not glow at night; 1 in daylight. The saturated luminance is raised
    // to PARTICLE_AMBIENT_LIGHT_POWER, so values >= 1 stay 1 and dim scenes fall faster.
    // Applied before GammaToLinear, so the linear result falls with twice that power.
    result.xyz *= ParticleAmbientLight();
#endif

    O.Color.xyz = GammaToLinear(result.xyz);
	
#ifdef USE_PBR
	O.Color.w = result.w;
#else
	O.Color.w = GammaToLinear(result.w);
#endif
	
	clip(O.Color.w - EPS);
	
	O.Velocity = 0.0f;
}

