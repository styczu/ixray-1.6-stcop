#include "stdafx.h"


#include "Blender_Particle.h"

#define					oBlendCount	6

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CBlender_Particle::CBlender_Particle()
{
	description.CLS		= B_PARTICLE;
	description.version	= 0;
	oBlend.Count		= oBlendCount;
	oBlend.IDselected	= 0;
	oAREF.value			= 32;
	oAREF.min			= 0;
	oAREF.max			= 255;
	oClamp.value		= TRUE;
}

CBlender_Particle::~CBlender_Particle()
{
	
}

void	CBlender_Particle::Save	( IWriter& fs	)
{
	IBlender::Save	(fs);

	// Blend mode
	xrP_TOKEN::Item	I;
	xrPWRITE_PROP	(fs,"Blending",	xrPID_TOKEN,     oBlend);
	I.ID = 0; xr_strcpy(I.str,"SET");			fs.w		(&I,sizeof(I));
	I.ID = 1; xr_strcpy(I.str,"BLEND");		fs.w		(&I,sizeof(I));
	I.ID = 2; xr_strcpy(I.str,"ADD");			fs.w		(&I,sizeof(I));
	I.ID = 3; xr_strcpy(I.str,"MUL");			fs.w		(&I,sizeof(I));
	I.ID = 4; xr_strcpy(I.str,"MUL_2X");		fs.w		(&I,sizeof(I));
	I.ID = 5; xr_strcpy(I.str,"ALPHA-ADD");	fs.w		(&I,sizeof(I));
	
	// Params
	xrPWRITE_PROP		(fs,"Texture clamp",xrPID_BOOL,		oClamp);
	xrPWRITE_PROP		(fs,"Alpha ref",	xrPID_INTEGER,	oAREF);
}

void	CBlender_Particle::Load	( IReader& fs, u16 version)
{
	IBlender::Load		(fs,version);

	xrPREAD_PROP		(fs,xrPID_TOKEN,		oBlend);	oBlend.Count =   oBlendCount;
	xrPREAD_PROP		(fs,xrPID_BOOL,			oClamp);
	xrPREAD_PROP		(fs,xrPID_INTEGER,		oAREF);
}

#if RENDER==R_R1
void	CBlender_Particle::Compile	(CBlender_Compile& C)
{
	IBlender::Compile		(C);
	switch (oBlend.IDselected)
	{
		case 0:	C.r_Pass	("particle","particle",TRUE,	TRUE,TRUE,	FALSE,	D3DBLEND_ONE,		D3DBLEND_ZERO,			TRUE,200);	break;	// SET
		case 1: C.r_Pass	("particle","particle",FALSE,	TRUE,FALSE,	TRUE,	D3DBLEND_SRCALPHA,	D3DBLEND_INVSRCALPHA,	TRUE,0);	break;	// BLEND
		case 2:	C.r_Pass	("particle","particle",FALSE,	TRUE,FALSE,	TRUE,	D3DBLEND_ONE,		D3DBLEND_ONE,			TRUE,0);	break;	// ADD
		case 3:	C.r_Pass	("particle","particle",FALSE,	TRUE,FALSE,	TRUE,	D3DBLEND_DESTCOLOR,	D3DBLEND_ZERO,			TRUE,0);	break;	// MUL
		case 4:	C.r_Pass	("particle","particle",FALSE,	TRUE,FALSE,	TRUE,	D3DBLEND_DESTCOLOR,	D3DBLEND_SRCCOLOR,		TRUE,0);	break;	// MUL_2X
		case 5:	C.r_Pass	("particle","particle",FALSE,	TRUE,FALSE,	TRUE,	D3DBLEND_SRCALPHA,	D3DBLEND_ONE,			TRUE,0);	break;	// ALPHA-ADD
	}
	C.r_Sampler				("s_base",	C.L_textures[0],false,oClamp.value?D3DTADDRESS_CLAMP:D3DTADDRESS_WRAP);
	C.r_End					();
}	
#elif RENDER==R_R2
void	CBlender_Particle::Compile	(CBlender_Compile& C)
{
	IBlender::Compile		(C);
	switch	(C.iElement) 
	{
	case SE_R2_NORMAL_HQ: 	// deffer
	case SE_R2_NORMAL_LQ: 	// deffer
		switch (oBlend.IDselected)
		{
		case 0:	C.r_Pass	("deffer_particle",	"deffer_particle",	FALSE,	TRUE,TRUE,	FALSE,	D3DBLEND_ONE,		D3DBLEND_ZERO,			FALSE,200);	break;	// SET
		case 1: C.r_Pass	("particle",		"particle",			FALSE,	TRUE,FALSE,	TRUE,	D3DBLEND_SRCALPHA,	D3DBLEND_INVSRCALPHA,	TRUE,0);	break;	// BLEND
		case 2:	C.r_Pass	("particle",		"particle",			FALSE,	TRUE,FALSE,	TRUE,	D3DBLEND_ONE,		D3DBLEND_ONE,			TRUE,0);	break;	// ADD
		case 3:	C.r_Pass	("particle",		"particle",			FALSE,	TRUE,FALSE,	TRUE,	D3DBLEND_DESTCOLOR,	D3DBLEND_ZERO,			TRUE,0);	break;	// MUL
		case 4:	C.r_Pass	("particle",		"particle",			FALSE,	TRUE,FALSE,	TRUE,	D3DBLEND_DESTCOLOR,	D3DBLEND_SRCCOLOR,		TRUE,0);	break;	// MUL_2X
		case 5:	C.r_Pass	("particle",		"particle",			FALSE,	TRUE,FALSE,	TRUE,	D3DBLEND_SRCALPHA,	D3DBLEND_ONE,			TRUE,0);	break;	// ALPHA-ADD
		};
		C.r_Sampler			("s_base",	C.L_textures[0],false,oClamp.value?D3DTADDRESS_CLAMP:D3DTADDRESS_WRAP);
		//	Igor: soft particles
		C.r_Sampler			("s_position",	"$user$position");
		C.r_End				();
		break;
	case SE_R2_SHADOW:		// smap
		// HARD or SOFT: shadow-map
		switch (oBlend.IDselected)
		{
		case 0:	C.r_Pass	("particle",		"particle",			FALSE,	TRUE,TRUE,	FALSE,	D3DBLEND_ONE,		D3DBLEND_ZERO,	TRUE,200);	break;	// SET
		case 1: C.r_Pass	("particle-clip",	"particle_s-blend",	FALSE,	TRUE,FALSE,	TRUE,	D3DBLEND_DESTCOLOR,	D3DBLEND_ZERO,	TRUE,0);	break;	// BLEND
		case 2:	C.r_Pass	("particle-clip",	"particle_s-add",	FALSE,	TRUE,FALSE,	TRUE,	D3DBLEND_DESTCOLOR,	D3DBLEND_ZERO,	TRUE,0);	break;	// ADD
		case 3:	C.r_Pass	("particle-clip",	"particle_s-mul",	FALSE,	TRUE,FALSE,	TRUE,	D3DBLEND_DESTCOLOR,	D3DBLEND_ZERO,	TRUE,0);	break;	// MUL
		case 4:	C.r_Pass	("particle-clip",	"particle_s-mul",	FALSE,	TRUE,FALSE,	TRUE,	D3DBLEND_DESTCOLOR,	D3DBLEND_ZERO,	TRUE,0);	break;	// MUL_2X
		case 5:	C.r_Pass	("particle-clip",	"particle_s-aadd",	FALSE,	TRUE,FALSE,	TRUE,	D3DBLEND_DESTCOLOR,	D3DBLEND_ZERO,	TRUE,0);	break;	// ALPHA-ADD
		};
		C.r_Sampler			("s_base",	C.L_textures[0],false,oClamp.value?D3DTADDRESS_CLAMP:D3DTADDRESS_WRAP);
		//	Igor: soft particles
		C.r_Sampler			("s_position",	"$user$position");
		C.r_End				();
		break;
	case 4: 	// deffer-EMAP
		break;
	};
}
#else

// Additive (ADD, ALPHA-ADD) particles are usually emissive (fire, sparks, glows). Only those whose
// texture names mark mist, splashes, smoke or dust follow the scene light. BLEND particles always do.
static bool IsAmbientLitParticleTexture(const char* TextureName)
{
	if (!TextureName)
		return false;

	string256 Lower;
	xr_strcpy(Lower, TextureName);
	xr_strlwr(Lower);

	// Emissive effects whose texture names also contain a token below (fireball_..._no_smoke,
	// firey_smoke, explosions\..., puffcolorsplash glows).
	static constexpr const char* Excluded[] = { "fire", "explo", "flame", "glow", "spark", "puffcolor" };
	for (const char* Token : Excluded)
	{
		if (strstr(Lower, Token))
			return false;
	}

	static constexpr const char* Tokens[] = { "splash", "water", "smoke", "dust", "mist", "fog", "steam" };
	for (const char* Token : Tokens)
	{
		if (strstr(Lower, Token))
			return true;
	}

	return false;
}

void CBlender_Particle::Compile(CBlender_Compile& C)
{
	IBlender::Compile(C);

	switch (C.iElement)
	{
		case SE_R2_HUD:
		case SE_R2_NORMAL_HQ:
		case SE_R2_NORMAL_LQ:
		{
			if (C.iElement != SE_R2_NORMAL_HQ)
			{
				RImplementation.addShaderOption("DISABLE_SOFT_PARTICLES", "1");
			}

			const bool AmbientLit = IsAmbientLitParticleTexture(C.L_textures.empty() ? nullptr : C.L_textures[0].c_str());

			switch (oBlend.IDselected)
			{
				case 0:	C.r_Pass("particle", "particle", false, true, false);	break;	// SET
				case 1:	RImplementation.addShaderOption("USE_PARTICLE_AMBIENT_LIGHT", "1");
					C.r_Pass("particle", "particle", FALSE, TRUE, FALSE, TRUE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA, TRUE, 0);	break;	// BLEND
				case 2:	if (AmbientLit) RImplementation.addShaderOption("USE_PARTICLE_AMBIENT_LIGHT", "1");
					C.r_Pass("particle", "particle_add", FALSE, TRUE, FALSE, TRUE, D3DBLEND_ONE, D3DBLEND_ONE, TRUE, 0);	break;	// ADD
				case 3:	C.r_Pass("particle", "particle", FALSE, TRUE, FALSE, TRUE, D3DBLEND_DESTCOLOR, D3DBLEND_ZERO, TRUE, 0);	break;	// MUL
				case 4:	C.r_Pass("particle", "particle", FALSE, TRUE, FALSE, TRUE, D3DBLEND_DESTCOLOR, D3DBLEND_SRCCOLOR, TRUE, 0);	break;	// MUL_2X
				case 5:	if (AmbientLit) RImplementation.addShaderOption("USE_PARTICLE_AMBIENT_LIGHT", "1");
					C.r_Pass("particle", "particle_add", FALSE, TRUE, FALSE, TRUE, D3DBLEND_SRCALPHA, D3DBLEND_ONE, TRUE, 0);	break;	// ALPHA-ADD
			};

			C.r_dx10Texture("s_base", C.L_textures[0]);
			C.r_dx10Texture("s_position", "$user$position");

			C.r_dx10Sampler("smp_base");
			C.r_dx10Sampler("smp_linear");
			C.r_dx10Sampler("smp_nofilter");

			C.r_End();
			break;
		}
	};
}
#endif
