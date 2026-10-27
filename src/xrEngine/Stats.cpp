#include "stdafx.h"
#include "GameFont.h"
#include "../xrCore/Collision/ISpatial.h"
#include "IGame_Persistent.h"
#include "Render.h"
#include "xr_object.h"

#include "../Include/xrRender/DrawUtils.h"

int g_ErrorLineCount = 15;
Flags32 g_stats_flags = {0};

ENGINE_API bool g_bDisableRedText = false;
ENGINE_API bool g_stats_log = false;

namespace
{
	// rs_stats_log: once a second, one CSV row with the per-frame average and maximum
	// of the rs_stats timers, so a stutter shows up as a spike in the maximum.
	struct StatsLogTimer
	{
		const char* name;
		CStatTimer CStats::* timer;
	};

	const StatsLogTimer s_log_timers[] =
	{
		{"engine", &CStats::EngineTOTAL},
		{"sheduler", &CStats::Sheduler},
		{"update_cl", &CStats::UpdateClient},
		{"physics", &CStats::Physics},
		{"ph_collision", &CStats::ph_collision},
		{"ph_core", &CStats::ph_core},
		{"animation", &CStats::Animation},
		{"ai_think", &CStats::AI_Think},
		{"ai_range", &CStats::AI_Range},
		{"ai_path", &CStats::AI_Path},
		{"ai_node", &CStats::AI_Node},
		{"ai_vis", &CStats::AI_Vis},
		{"ai_vis_query", &CStats::AI_Vis_Query},
		{"ai_vis_ray", &CStats::AI_Vis_RayTests},
		{"render", &CStats::RenderTOTAL},
		{"r_calc", &CStats::RenderCALC},
		{"r_hom", &CStats::RenderCALC_HOM},
		{"r_dump", &CStats::RenderDUMP},
		{"r_rt", &CStats::RenderDUMP_RT},
		{"r_skin", &CStats::RenderDUMP_SKIN},
		{"r_wait", &CStats::RenderDUMP_Wait},
		{"r_wait_s", &CStats::RenderDUMP_Wait_S},
		{"r_hud", &CStats::RenderDUMP_HUD},
		{"r_glows", &CStats::RenderDUMP_Glows},
		{"r_lights", &CStats::RenderDUMP_Lights},
		{"r_wallmarks", &CStats::RenderDUMP_WM},
		{"r_dt_vis", &CStats::RenderDUMP_DT_VIS},
		{"r_dt_render", &CStats::RenderDUMP_DT_Render},
		{"r_dt_cache", &CStats::RenderDUMP_DT_Cache},
		{"r_pcalc", &CStats::RenderDUMP_Pcalc},
		{"r_scalc", &CStats::RenderDUMP_Scalc},
		{"r_srender", &CStats::RenderDUMP_Srender},
		{"sound", &CStats::Sound},
		{"cl_ray", &CStats::clRAY},
		{"cl_box", &CStats::clBOX},
	};

	constexpr size_t s_log_count = sizeof(s_log_timers) / sizeof(s_log_timers[0]);

	struct StatsLogState
	{
		FILE* file = nullptr;
		CTimer clock;
		float last_frame = 0.f;
		u32 frames = 0;
		float frame_sum = 0.f;
		float frame_max = 0.f;
		float sum[s_log_count] = {};
		float max[s_log_count] = {};
		u64 calls[s_log_count] = {};
		u64 wallmarks_static = 0;
		u64 wallmarks_dynamic = 0;
		u64 wallmarks_tris = 0;
		u64 details = 0;
		u64 clients_active = 0;
		u64 particles_active = 0;
	};

	StatsLogState s_log;
	float s_log_time = 0.f;

	void StatsLogReset()
	{
		FILE* file = s_log.file;
		s_log = StatsLogState();
		s_log.file = file;
		s_log.clock.Start();
	}

	void StatsLogClose()
	{
		if (s_log.file)
		{
			fclose(s_log.file);
			s_log.file = nullptr;
		}
	}

	void StatsLogFrame(const CStats& stats)
	{
		if (!g_stats_log)
		{
			StatsLogClose();
			return;
		}

		if (!s_log.file)
		{
			string64 stamp;
			Time time;
			xr_sprintf(stamp, "%s.%s.%s-%s.%s.%s", time.GetYearString().c_str(), time.GetMonthString().c_str(),
				time.GetDayString().c_str(), time.GetHoursString().c_str(), time.GetMinutesString().c_str(),
				time.GetSecondsString().c_str());

			string_path name, path;
			xr_sprintf(name, "ixray-stats-%s.csv", stamp);
			FS.update_path(path, "$logs$", name);
			s_log.file = fopen(path, "w");
			if (!s_log.file)
			{
				Msg("! rs_stats_log: can't open [%s]", path);
				g_stats_log = false;
				return;
			}

			Msg("* rs_stats_log: writing [%s]", path);
			fprintf(s_log.file, "time_s,frames,fps,frame_avg_ms,frame_max_ms");
			for (const StatsLogTimer& column : s_log_timers)
			{
				fprintf(s_log.file, ",%s_avg_ms,%s_max_ms,%s_calls", column.name, column.name, column.name);
			}
			fprintf(s_log.file, ",wm_static,wm_dynamic,wm_tris,details,clients_active,particles_active\n");
			fflush(s_log.file);
			s_log_time = 0.f;
			StatsLogReset();
			return;
		}

		const float now = s_log.clock.GetElapsed_sec();
		const float frame_ms = (now - s_log.last_frame) * 1000.f;
		s_log.last_frame = now;

		s_log.frames++;
		s_log.frame_sum += frame_ms;
		s_log.frame_max = std::max(s_log.frame_max, frame_ms);

		for (size_t i = 0; i < s_log_count; ++i)
		{
			const CStatTimer& timer = stats.*(s_log_timers[i].timer);
			s_log.sum[i] += timer.result;
			s_log.max[i] = std::max(s_log.max[i], timer.result);
			s_log.calls[i] += timer.count;
		}

		s_log.wallmarks_static += stats.RenderDUMP_WMS_Count;
		s_log.wallmarks_dynamic += stats.RenderDUMP_WMD_Count;
		s_log.wallmarks_tris += stats.RenderDUMP_WMT_Count;
		s_log.details += stats.RenderDUMP_DT_Count;
		s_log.clients_active += stats.UpdateClient_active;
		s_log.particles_active += stats.Particles_active;

		if (now < 1.f)
		{
			return;
		}

		const float frames = float(s_log.frames);
		s_log_time += now;

		fprintf(s_log.file, "%.1f,%u,%.1f,%.2f,%.2f", s_log_time, s_log.frames, frames / now, s_log.frame_sum / frames,
			s_log.frame_max);
		for (size_t i = 0; i < s_log_count; ++i)
		{
			fprintf(s_log.file, ",%.3f,%.3f,%.1f", s_log.sum[i] / frames, s_log.max[i], float(s_log.calls[i]) / frames);
		}
		fprintf(s_log.file, ",%.0f,%.0f,%.0f,%.0f,%.0f,%.0f\n", float(s_log.wallmarks_static) / frames,
			float(s_log.wallmarks_dynamic) / frames, float(s_log.wallmarks_tris) / frames,
			float(s_log.details) / frames, float(s_log.clients_active) / frames,
			float(s_log.particles_active) / frames);
		fflush(s_log.file);
		StatsLogReset();
	}
}

CStats::CStats	()
{
	fFPS = 0.f;
	fRFPS = 0.f;
	fTPS = 0.f;
	pFont = nullptr;
	fMem_calls = 0.f;
	RenderDUMP_DT_Count = 0.f;
	Device.seqRender.Add(this, REG_PRIORITY_LOW - 1000);
}

CStats::~CStats()
{
	Device.seqRender.Remove		(this);
	xr_delete		(pFont);
}

void _draw_cam_pos(CGameFont* pFont)
{
	float sz		= pFont->GetHeight();
	pFont->SetColor	(0xffffffff);
	pFont->Out		(10, 600, "CAMERA POSITION:  [%3.2f,%3.2f,%3.2f]",VPUSH(Device.vCameraPosition));
	pFont->SetHeight(sz);
	pFont->OnRender	();
}

void CStats::Show() 
{
	// Stop timers
	{
		EngineTOTAL.FrameEnd		();	
		Sheduler.FrameEnd			();	
		UpdateClient.FrameEnd		();	
		Physics.FrameEnd			();	
		ph_collision.FrameEnd		();
		ph_core.FrameEnd			();
		Animation.FrameEnd			();	
		AI_Think.FrameEnd			();
		AI_Range.FrameEnd			();
		AI_Path.FrameEnd			();
		AI_Node.FrameEnd			();
		AI_Vis.FrameEnd				();
		AI_Vis_Query.FrameEnd		();
		AI_Vis_RayTests.FrameEnd	();
		
		RenderTOTAL.FrameEnd		();
		RenderCALC.FrameEnd			();
		RenderCALC_HOM.FrameEnd		();
		RenderDUMP.FrameEnd			();	
		RenderDUMP_RT.FrameEnd		();
		RenderDUMP_SKIN.FrameEnd	();	
		RenderDUMP_Wait.FrameEnd	();	
		RenderDUMP_Wait_S.FrameEnd	();	
		RenderDUMP_HUD.FrameEnd		();	
		RenderDUMP_Glows.FrameEnd	();	
		RenderDUMP_Lights.FrameEnd	();	
		RenderDUMP_WM.FrameEnd		();	
		RenderDUMP_DT_VIS.FrameEnd	();	
		RenderDUMP_DT_Render.FrameEnd();	
		RenderDUMP_DT_Cache.FrameEnd();
		RenderDUMP_Pcalc.FrameEnd	();	
		RenderDUMP_Scalc.FrameEnd	();	
		RenderDUMP_Srender.FrameEnd	();	
		
		Sound.FrameEnd				();
		Input.FrameEnd				();
		clRAY.FrameEnd				();	
		clBOX.FrameEnd				();
		clFRUSTUM.FrameEnd			();
		
		netClient1.FrameEnd			();
		netClient2.FrameEnd			();
		netServer.FrameEnd			();

		netClientCompressor.FrameEnd();
		netServerCompressor.FrameEnd();
		
		TEST0.FrameEnd				();
		TEST1.FrameEnd				();
		TEST2.FrameEnd				();
		TEST3.FrameEnd				();
	}
	
	if (Device.fTimeDelta > EPS_S)
	{
		constexpr float smoothing_alpha = 0.10f;
		
		float raw_fps = 1.f / Device.fTimeDelta;
		fFPS = raw_fps * smoothing_alpha + fFPS * (1.f - smoothing_alpha);

		float fOne = 0.3f;
		float fInv = 1.f-fOne;
		
		if (RenderTOTAL.result > EPS_S)
		{
			u32	rendered_polies = Device.m_pRender->GetCacheStatPolys();
			fTPS = fInv*fTPS + fOne*float(rendered_polies)/(RenderTOTAL.result*1000.f);
			//fTPS = fInv*fTPS + fOne*float(RCache.stat.polys)/(RenderTOTAL.result*1000.f);
			fRFPS= fInv*fRFPS+ fOne*1000.f/RenderTOTAL.result;
		}
	}
	
	{
		float mem_count		= float	(Memory.stat_calls);
		if (mem_count>fMem_calls)	fMem_calls	=	mem_count;
		else						fMem_calls	=	.9f*fMem_calls + .1f*mem_count;
		Memory.stat_calls	= 0		;
	}

	StatsLogFrame(*this);
	
	if (g_dedicated_server)
		return;
	
	int frm = 2000;
	div_t ddd = div(Device.dwFrame,frm);
	
	// rs_stats_log alone calls Show() for the timers only, without the overlay
	const bool overlay = psDeviceFlags.test(rsCameraPos) || psDeviceFlags.test(rsStatistic) || !errors.empty();
	if (overlay && ddd.rem < frm/2.0f)
		{
		pFont->SetColor	(0xFFFFFFFF	);
		pFont->OutSet	(0,0);
		pFont->OutNext	(*eval_line_1);
		pFont->OutNext	(*eval_line_2);
		pFont->OutNext	(*eval_line_3);
		pFont->OnRender	();
	}

	CGameFont& F = *pFont;
	float f_base_size = 15.0f;
	F.SetHeight(f_base_size);
	
	if (psDeviceFlags.test(rsStatistic))
	{
		static float	r_ps		= 0;
		static float	b_ps		= 0;
		r_ps						= .99f*r_ps + .01f*(clRAY.count/clRAY.result);
		b_ps						= .99f*b_ps + .01f*(clBOX.count/clBOX.result);

		CSound_stats				snd_stat;
		::Sound->statistic			(&snd_stat,0);
		F.SetColor	(0xFFFFFFFF	);

		F.OutSet	(10,35);
		F.OutNext	("FPS/RFPS:    %4.0f/%4.0f",fFPS,fRFPS);
		F.OutNext	("TPS:         %4.0f M",	fTPS);
		m_pRender->OutData1(F);
		m_pRender->OutData3(F);
		F.OutSkip	();

#define PPP(a) (100.f*float(a)/float(EngineTOTAL.result))
		F.OutNext	("*** ENGINE:  %2.2fms",EngineTOTAL.result);	
		F.OutNext	("Memory:      %2.2fa",fMem_calls);
		F.OutNext	("uClients:    %2.2fms, %2.1f%%, crow(%d)/active(%d)/total(%d)",UpdateClient.result,PPP(UpdateClient.result),UpdateClient_crows,UpdateClient_active,UpdateClient_total);
		F.OutNext	("uSheduler:   %2.2fms, %2.1f%%",Sheduler.result,		PPP(Sheduler.result));
		F.OutNext	("uSheduler_L: %2.2fms",fShedulerLoad);
		F.OutNext	("uParticles:  Qstart[%d] Qactive[%d] Qdestroy[%d]",	Particles_starting,Particles_active,Particles_destroy);
		F.OutNext	("Physics:     %2.2fms, %2.1f%%",Physics.result,		PPP(Physics.result));	
		F.OutNext	("  collider:  %2.2fms", ph_collision.result);	
		F.OutNext	("  solver:    %2.2fms, %d",ph_core.result,ph_core.count);	
		F.OutNext	("aiThink:     %2.2fms, %d",AI_Think.result,AI_Think.count);	
		F.OutNext	("  aiRange:   %2.2fms, %d",AI_Range.result,AI_Range.count);
		F.OutNext	("  aiPath:    %2.2fms, %d",AI_Path.result,AI_Path.count);
		F.OutNext	("  aiNode:    %2.2fms, %d",AI_Node.result,AI_Node.count);
		F.OutNext	("aiVision:    %2.2fms, %d",AI_Vis.result,AI_Vis.count);
		F.OutNext	("  Query:     %2.2fms",	AI_Vis_Query.result);
		F.OutNext	("  RayCast:   %2.2fms",	AI_Vis_RayTests.result);
		F.OutSkip	();
								   
#undef  PPP
#define PPP(a) (100.f*float(a)/float(RenderTOTAL.result))
		F.OutNext	("*** RENDER:  %2.2fms",RenderTOTAL.result);
		F.OutNext	("R_CALC:      %2.2fms, %2.1f%%",RenderCALC.result,	PPP(RenderCALC.result));	
		F.OutNext	("  HOM:       %2.2fms, %d",RenderCALC_HOM.result,	RenderCALC_HOM.count);
		F.OutNext	("  Skeletons: %2.2fms, %d",Animation.result,		Animation.count);
		F.OutNext	("R_DUMP:      %2.2fms, %2.1f%%",RenderDUMP.result,	PPP(RenderDUMP.result));	
		F.OutNext	("  Wait-L:    %2.2fms",RenderDUMP_Wait.result);	
		F.OutNext	("  Wait-S:    %2.2fms",RenderDUMP_Wait_S.result);	
		F.OutNext	("  Skinning:  %2.2fms",RenderDUMP_SKIN.result);	
		F.OutNext	("  DT_Vis/Cnt:%2.2fms/%d",RenderDUMP_DT_VIS.result,RenderDUMP_DT_Count);	
		F.OutNext	("  DT_Render: %2.2fms",RenderDUMP_DT_Render.result);	
		F.OutNext	("  DT_Cache:  %2.2fms",RenderDUMP_DT_Cache.result);	
		F.OutNext	("  Wallmarks: %2.2fms, %d/%d - %d",RenderDUMP_WM.result,RenderDUMP_WMS_Count,RenderDUMP_WMD_Count,RenderDUMP_WMT_Count);
		F.OutNext	("  Glows:     %2.2fms",RenderDUMP_Glows.result);	
		F.OutNext	("  Lights:    %2.2fms, %d",RenderDUMP_Lights.result,RenderDUMP_Lights.count);
		F.OutNext	("  RT:        %2.2fms, %d",RenderDUMP_RT.result,RenderDUMP_RT.count);
		F.OutNext	("  HUD:       %2.2fms",RenderDUMP_HUD.result);	
		F.OutNext	("  P_calc:    %2.2fms",RenderDUMP_Pcalc.result);
		F.OutNext	("  S_calc:    %2.2fms",RenderDUMP_Scalc.result);
		F.OutNext	("  S_render:  %2.2fms, %d",RenderDUMP_Srender.result,RenderDUMP_Srender.count);
		F.OutSkip	();
		F.OutSet(500, 35);
		F.OutNext	("*** SOUND:   %2.2fms",Sound.result);
		F.OutNext	("  TGT/SIM/E: %d/%d/%d",  snd_stat._rendered, snd_stat._simulated, snd_stat._events);
		F.OutNext	("  HIT/MISS:  %d/%d",  snd_stat._cache_hits, snd_stat._cache_misses);
		F.OutSkip	();
		F.OutNext	("Input:       %2.2fms",Input.result);
		F.OutNext	("clRAY:       %2.2fms, %d, %2.0fK",clRAY.result,		clRAY.count,r_ps);
		F.OutNext	("clBOX:       %2.2fms, %d, %2.0fK",clBOX.result,		clBOX.count,b_ps);
		F.OutNext	("clFRUSTUM:   %2.2fms, %d",		clFRUSTUM.result,	clFRUSTUM.count	);
		F.OutSkip	();
		F.OutNext	("netClientRecv:   %2.2fms, %d",	netClient1.result,netClient1.count);
		F.OutNext	("netClientSend:   %2.2fms, %d",	netClient2.result,netClient2.count);
		F.OutNext	("netServer:   %2.2fms, %d",		netServer.result,netServer.count);
		F.OutNext	("netClientCompressor:   %2.2fms",	netClientCompressor.result);
		F.OutNext	("netServerCompressor:   %2.2fms",	netServerCompressor.result);
		
		F.OutSkip	();

		F.OutSkip	();
		F.OutNext	("TEST 0:      %2.2fms, %d",TEST0.result,TEST0.count);
		F.OutNext	("TEST 1:      %2.2fms, %d",TEST1.result,TEST1.count);
		F.OutNext	("TEST 2:      %2.2fms, %d",TEST2.result,TEST2.count);
		F.OutNext	("TEST 3:      %2.2fms, %d",TEST3.result,TEST3.count);
		F.OutSkip	();
		F.OutNext	("qpc[%3d]",CPU::qpc_counter);
		
		CPU::qpc_counter	=	0		;
		F.OutSkip	();
		m_pRender->OutData4(F);
		//////////////////////////////////////////////////////////////////////////
		// Renderer specific
		F.OutSet						(200,0);
		Render->Statistics				(&F);

		//////////////////////////////////////////////////////////////////////////
		// Game specific
		F.OutSet						(400,0);
		g_pGamePersistent->Statistics	(&F);

		//////////////////////////////////////////////////////////////////////////
		// process PURE STATS
		seqStats.Process<&pureStats::OnStats>();
		pFont->OnRender					();
	};

	if( /*psDeviceFlags.test(rsStatistic) ||*/ psDeviceFlags.test(rsCameraPos) ){
		_draw_cam_pos					(pFont);
		pFont->OnRender					();
	};
#ifdef DEBUG
	//////////////////////////////////////////////////////////////////////////
	// PERF ALERT
	if (!g_bDisableRedText)
	{
		CGameFont&	F_ = *((CGameFont*)pFont);
		F_.SetColor						(color_rgba(255,16,16,255));
		F_.OutSet						(300,360);
		if (fFPS<30)					F_.OutNext	("FPS       < 30:   %3.1f",	fFPS);
		//if (RCache.stat.verts>500000)	F.OutNext	("Verts     > 500k: %d",	RCache.stat.verts);
		m_pRender->GuardVerts(F_);
		////if (RCache.stat.polys>500000)	F.OutNext	("Polys     > 500k: %d",	RCache.stat.polys);
		if (psDeviceFlags.test(rsStatistic))
		{
			m_pRender->GuardDrawCalls(F_);
			//if (RCache.stat.calls>1000)		F.OutNext	("DIP/DP    > 1k:   %d",	RCache.stat.calls);
			////if (RCache.stat.textures>1000)F.OutNext	("T_change  > 500:  %d",	RCache.stat.textures);
			if (RenderDUMP_DT_Count>1000)	F_.OutNext	("DT_count  > 1000: %u",	RenderDUMP_DT_Count);
			F_.OutSkip						();
			//if (fMem_calls>1500)			F.OutNext	("MMGR calls > 1500:%3.1f",	fMem_calls);
			if (Sheduler.result>3.f)		F_.OutNext	("Update     > 3ms:	%3.1f",	Sheduler.result);
			if (UpdateClient.result>3.f)	F_.OutNext	("UpdateCL   > 3ms: %3.1f",	UpdateClient.result);
			if (Physics.result>5.f)			F_.OutNext	("Physics    > 5ms: %3.1f",	Physics.result);	
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Show errors
	if (!g_bDisableRedText && errors.size())
	{
		CGameFont&	F_ = *((CGameFont*)pFont);
		F_.SetColor	(color_rgba(255,16,16,191));
		F_.OutSet	(750,35);
#if 0
		for (u32 it=0; it<errors.size(); it++)
			F_.OutNext("%s",errors[it].c_str());
#else
		for (u32 it=(u32)std::max(int(0),(int)errors.size() - g_ErrorLineCount); it<errors.size(); it++)
			F_.OutNext("%s",errors[it].c_str());
#endif
		F_.OnRender	();
	}
#endif

	{
		EngineTOTAL.FrameStart		();	
		Sheduler.FrameStart			();	
		UpdateClient.FrameStart		();	
		Physics.FrameStart			();	
		ph_collision.FrameStart		();
		ph_core.FrameStart			();
		Animation.FrameStart		();	
		AI_Think.FrameStart			();
		AI_Range.FrameStart			();
		AI_Path.FrameStart			();
		AI_Node.FrameStart			();
		AI_Vis.FrameStart			();
		AI_Vis_Query.FrameStart		();
		AI_Vis_RayTests.FrameStart	();
		
		RenderTOTAL.FrameStart		();
		RenderCALC.FrameStart		();
		RenderCALC_HOM.FrameStart	();
		RenderDUMP.FrameStart		();	
		RenderDUMP_RT.FrameStart	();
		RenderDUMP_SKIN.FrameStart	();	
		RenderDUMP_Wait.FrameStart	();	
		RenderDUMP_Wait_S.FrameStart();	
		RenderDUMP_HUD.FrameStart	();	
		RenderDUMP_Glows.FrameStart	();	
		RenderDUMP_Lights.FrameStart();	
		RenderDUMP_WM.FrameStart	();	
		RenderDUMP_DT_VIS.FrameStart();	
		RenderDUMP_DT_Render.FrameStart();	
		RenderDUMP_DT_Cache.FrameStart();	
		RenderDUMP_Pcalc.FrameStart	();	
		RenderDUMP_Scalc.FrameStart	();	
		RenderDUMP_Srender.FrameStart();	
		
		Sound.FrameStart			();
		Input.FrameStart			();
		clRAY.FrameStart			();	
		clBOX.FrameStart			();
		clFRUSTUM.FrameStart		();
		
		netClient1.FrameStart		();
		netClient2.FrameStart		();
		netServer.FrameStart		();
		netClientCompressor.FrameStart();
		netServerCompressor.FrameStart();

		TEST0.FrameStart			();
		TEST1.FrameStart			();
		TEST2.FrameStart			();
		TEST3.FrameStart			();
	}
	dwSND_Played = dwSND_Allocated = 0;
	Particles_starting = Particles_active = Particles_destroy = 0;
}

void	_LogCallback(const char* string)
{
	if (string && '!' == string[0] && ' ' == string[1])
		Device.Statistic->errors.emplace_back(string);
}

void CStats::OnDeviceCreate			()
{
	g_bDisableRedText = Core.ParamsData.test(ECoreParams::xclsx);

	if (!g_dedicated_server) {
		pFont = g_FontManager->GetFont("stat_font", CGameFont::fsDeviceIndependent);// new CGameFont("stat_font", CGameFont::fsDeviceIndependent);
	}
	
	if(!pSettings->section_exist("evaluation")
		||!pSettings->line_exist("evaluation","line1")
		||!pSettings->line_exist("evaluation","line2")
		||!pSettings->line_exist("evaluation","line3") )
		FATAL	("");

	eval_line_1 = pSettings->r_string_wb("evaluation","line1");
	eval_line_2 = pSettings->r_string_wb("evaluation","line2");
	eval_line_3 = pSettings->r_string_wb("evaluation","line3");

	// 
#ifdef DEBUG
	if (!g_bDisableRedText)
		xrLogger::AddLogCallback(_LogCallback);
#endif
}

void CStats::OnDeviceDestroy		()
{
	StatsLogClose();
	xrLogger::RemoveLogCallback(_LogCallback);
	xr_delete	(pFont);
}

void CStats::OnRender()
{
#ifdef DEBUG_DRAW
	if (g_stats_flags.is(st_sound)){
		CSound_stats_ext				snd_stat_ext;
		::Sound->statistic				(0,&snd_stat_ext);
		CSound_stats_ext::item_vec_it	_I = snd_stat_ext.items.begin();
		CSound_stats_ext::item_vec_it	_E = snd_stat_ext.items.end();
		for (;_I!=_E;_I++){
			const CSound_stats_ext::SItem& item = *_I;
			if (item._3D)
			{
				m_pRender->SetDrawParams(&*Device.m_pRender);
				//RCache.set_xform_world(Fidentity);
				//RCache.set_Shader		(Device.m_SelectionShader);
				//RCache.set_c			("tfactor",1,1,1,1);
				DU->DrawCross			(item.params.position, 0.5f, 0xFF0000FF, true );
				if (g_stats_flags.is(st_sound_min_dist))
					DU->DrawSphere		(Fidentity, item.params.position, item.params.min_distance, 0x400000FF,	0xFF0000FF, true, true);
				if (g_stats_flags.is(st_sound_max_dist))
					DU->DrawSphere		(Fidentity, item.params.position, item.params.max_distance, 0x4000FF00,	0xFF008000, true, true);

				xr_string out_txt = (item.name.size() && g_stats_flags.is(st_sound_info_name)) ? item.name.c_str() : "";

				if (item.game_object)
				{
					if (g_stats_flags.is(st_sound_ai_dist))
						DU->DrawSphere	(Fidentity, item.params.position, item.params.max_ai_distance, 0x80FF0000,0xFF800000,true,true);
					if (g_stats_flags.is(st_sound_info_object)){
						out_txt			+= "  (";
						out_txt			+= item.game_object->cNameSect().c_str();
						out_txt			+= ")";
					}
				}
				if (g_stats_flags.is_any(st_sound_info_name|st_sound_info_object) && item.name.size())
					DU->OutText			(item.params.position, out_txt.c_str(),0xFFFFFFFF,0xFF000000);
			}
		}
	}
#endif
	
	if (psDeviceFlags.test(rsCameraPos) || psDeviceFlags.test(rsStatistic) || !Device.Statistic->errors.empty() || g_stats_log || s_log.file)
	{
		Device.Statistic->Show();
	}
	
	Device.Statistic->RenderTOTAL_Real.End();
	Device.Statistic->RenderTOTAL_Real.FrameEnd();
	Device.Statistic->RenderTOTAL.accum = Device.Statistic->RenderTOTAL_Real.accum;
}
