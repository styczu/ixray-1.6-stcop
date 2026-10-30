#include "stdafx.h"
#include "r4_rendertarget.h"

bool CRenderTarget::phase_puddles()
{
	auto& wetness_factor = g_pGamePersistent->Environment().wetness_factor;

	if(RImplementation.m_levels_puddles.empty() || wetness_factor == 0.0f)
	{
		return false;
	}

	auto puddles = 0;

	for(CRender::PuddleBase& puddle : RImplementation.m_levels_puddles) 
	{
		if(!RImplementation.ViewBase.testSphere_dirty(puddle.m_world.c, puddle.m_radius))
		{
			continue;
		}

		// Set the water state only when a puddle is drawn: setting it with no
		// puddle in view (and so without the special pass that follows a drawn
		// one) left the frame dark grey in rain at some view angles
		if(puddles == 0)
		{
			GRHI->StateManager->SetCullMode(ERHI_CULLMODE::BACK);
			RCache.set_Shader(s_puddles);
		}

		RCache.set_xform_world(puddle.m_world);

		RCache.set_c("puddle_constants", g_pGamePersistent->Environment().wetness_factor * puddle.m_height);
		RCache.Render_noIA(6);

		++puddles;
	}

	return puddles > 0;
}
