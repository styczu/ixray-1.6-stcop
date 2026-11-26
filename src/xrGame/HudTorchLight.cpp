#include "StdAfx.h"
#include "HudTorchLight.h"
#include "player_hud.h"
#include "CustomDevice.h"
#include "Weapon.h"
#include "Actor.h"
#include "Inventory.h"
#include "../debug_renderer.h"
#include "ElectronicsProblemsManager.h"
#include "../xrEngine/xr_ioc_cmd.h"
#include "../xrEngine/IGame_Persistent.h"
#include "../xrEngine/IGame_Menu.h"
#include "../Include/xrRender/FactoryPtr.h"
#include "../Include/xrRender/UIRender.h"
#include "../Include/xrRender/UIShader.h"
#include <algorithm>
#include <cmath>

namespace
{
	void ClearActiveLaserDotForOwner(const void* owner);
	void ClearLaserZeroCacheForOwner(const void* owner);
}

void THudLightTorch::BeginComponent(IECSOwner* O)
{
	SetInstalled(true);
}

void THudLightTorch::EndComponent()
{
	ClearActiveLaserDotForOwner(this);
	ClearLaserZeroCacheForOwner(this);

	if (RenderLight != nullptr)
	{
		RenderLight.destroy();
	}

	if (OmniLight != nullptr)
	{
		OmniLight.destroy();
	}
}

void THudLightTorch::NewTorchlight(const char* section)
{
	if (!IsTorchInstalled)
		return;

	if (RenderLight)
	{
		RenderLight.destroy();
	}

	if (OmniLight)
	{
		OmniLight.destroy();
	}

	Section = section;

	RenderLight = ::Render->light_create();
	RenderLight->set_type((IRender_Light::LT)READ_IF_EXISTS(pSettings, r_u32, section, "torch_render_type", IRender_Light::SPOT));
	RenderLight->set_range(READ_IF_EXISTS(pSettings, r_float, section, "torch_r2_range", 15.0f));
	RenderLight->set_shadow(!!READ_IF_EXISTS(pSettings, r_bool, section, "torch_render_shadow", true));

	OmniLight = ::Render->light_create();
	OmniLight->set_type(IRender_Light::POINT); // (IRender_Light::LT)READ_IF_EXISTS(pSettings, r_u32, section, "torch_omni_type", IRender_Light::POINT));
	OmniLight->set_range(READ_IF_EXISTS(pSettings, r_float, section, "torch_r2_omni_range", 0.75f));
	OmniLight->set_shadow(!!READ_IF_EXISTS(pSettings, r_bool, section, "torch_omni_shadow", false));

	LightBone = pSettings->r_string(section, "torch_light_bone");

	LightOffset.x = READ_IF_EXISTS(pSettings, r_float, section, "torch_attach_offset_x", 0.0f);
	LightOffset.y = READ_IF_EXISTS(pSettings, r_float, section, "torch_attach_offset_y", 0.0f);
	LightOffset.z = READ_IF_EXISTS(pSettings, r_float, section, "torch_attach_offset_z", 0.0f);

	AimOffset.x = READ_IF_EXISTS(pSettings, r_float, section, "torch_aim_attach_offset_x", 0.0f);
	AimOffset.y = READ_IF_EXISTS(pSettings, r_float, section, "torch_aim_attach_offset_y", 0.0f);
	AimOffset.z = READ_IF_EXISTS(pSettings, r_float, section, "torch_aim_attach_offset_z", 0.0f);

	LightWorldOffset.x = READ_IF_EXISTS(pSettings, r_float, section, "torch_world_attach_offset_x", 0.0f);
	LightWorldOffset.y = READ_IF_EXISTS(pSettings, r_float, section, "torch_world_attach_offset_y", 0.0f);
	LightWorldOffset.z = READ_IF_EXISTS(pSettings, r_float, section, "torch_world_attach_offset_z", 0.0f);

	OmniOffset.x = READ_IF_EXISTS(pSettings, r_float, section, "torch_omni_attach_offset_x", LightOffset.x);
	OmniOffset.y = READ_IF_EXISTS(pSettings, r_float, section, "torch_omni_attach_offset_y", LightOffset.y);
	OmniOffset.z = READ_IF_EXISTS(pSettings, r_float, section, "torch_omni_attach_offset_z", LightOffset.z);

	OmniWorldOffset.x = READ_IF_EXISTS(pSettings, r_float, section, "torch_omni_world_attach_offset_x", LightWorldOffset.x);
	OmniWorldOffset.y = READ_IF_EXISTS(pSettings, r_float, section, "torch_omni_world_attach_offset_y", LightWorldOffset.y);
	OmniWorldOffset.z = READ_IF_EXISTS(pSettings, r_float, section, "torch_omni_world_attach_offset_z", LightWorldOffset.z);

	LightColor.r = READ_IF_EXISTS(pSettings, r_float, section, "torch_r2_color_r", 0.60f);
	LightColor.g = READ_IF_EXISTS(pSettings, r_float, section, "torch_r2_color_g", 0.55f);
	LightColor.b = READ_IF_EXISTS(pSettings, r_float, section, "torch_r2_color_b", 0.55f);
	LightColor.a = READ_IF_EXISTS(pSettings, r_float, section, "torch_r2_color_a", 0.80f);
	RenderLight->set_color(LightColor);

	OmniColor.r = READ_IF_EXISTS(pSettings, r_float, section, "torch_r2_omni_color_r", 0.60f);
	OmniColor.g = READ_IF_EXISTS(pSettings, r_float, section, "torch_r2_omni_color_g", 0.55f);
	OmniColor.b = READ_IF_EXISTS(pSettings, r_float, section, "torch_r2_omni_color_b", 0.55f);
	OmniColor.a = READ_IF_EXISTS(pSettings, r_float, section, "torch_r2_omni_color_a", 0.80f);
	OmniLight->set_color(OmniColor);

	RenderLight->set_cone(deg2rad(READ_IF_EXISTS(pSettings, r_float, section, "torch_spot_angle", 75.0f)));
	RenderLight->set_texture(pSettings->r_string(section, "torch_spot_texture"));

	IsLightDirByBone = !!READ_IF_EXISTS(pSettings, r_bool, section, "light_directions_by_bones", false);
	if (IsLightDirByBone)
	{
		LightDirBoneName = pSettings->r_string(section, "light_dir_bone");
	}

	if (pSettings->line_exist(section, "torch_cone_bones"))
	{
		ConeBones.clear();
		string128 bone_name = {};

		const char* lineStr = pSettings->r_string(section, "torch_cone_bones");
		for (auto i = 0, cnt = _GetItemCount(lineStr); i < cnt; ++i)
		{
			ConeBones.push_back(_GetItem(lineStr, i, bone_name));
		}
	}

	Fvector tmp_vector = { -1.0f, -1.0f, 0.0f };

	tmp_vector = READ_IF_EXISTS(pSettings, r_fvector3, section, "torch_breaking_params", tmp_vector);
	BreakingParams.start_condition = tmp_vector.x;
	BreakingParams.end_condition = tmp_vector.y;
	BreakingParams.start_probability = tmp_vector.z;
}

void THudLightTorch::SwitchTorchlight(bool isActive)
{
	if (IsTorchInstalled)
	{
		IsRenderLight = isActive;
	}
	else
	{
		IsRenderLight = false;
	}
}

bool forceTPDraw = false;
void THudLightTorch::UpdateTorchFromObject(CHudItem* item) const
{
	if (RenderLight == nullptr || OmniLight == nullptr || item == nullptr || item->object().Visual() == nullptr)
	{
		return;
	}

	if (item->object().H_Parent())
	{
		if (item->GetState() == item->eHidden || item->cast_inventory_item()->CurrPlace() == eItemPlace::eItemPlaceRuck)
		{
			RenderLight->set_active(false);
			OmniLight->set_active(false);

			return;
		}
	}

	bool isHudMode = item->GetHUDmode() && item->HudItemData();
	if (IsRenderLight)
	{
		IKinematics* kin = nullptr;
		Fmatrix xform;
		Fvector lightPos = { 0, 0, 0 };
		Fvector lightDir = { 0, 0, 1 };
		Fvector omniPos = { 0, 0, 0 };
		u16 lightBoneId = BI_NONE;
		u16 lightDirBoneId = BI_NONE;
		Fvector up, right;

		if (isHudMode)
		{
			xform = item->HudItemData()->m_item_transform;
			kin = item->HudItemData()->m_model;
			lightBoneId = kin->LL_BoneID(LightBone);

			Fvector3 curr_light_offset = LightOffset;
			Fvector3 curr_omni_offset = OmniOffset;

			if (item->cast_custom_device() != nullptr)
			{
				PIItem active_item = Actor()->inventory().ActiveItem();
				CWeapon* wpn = active_item != nullptr ? active_item->cast_weapon() : nullptr;
				if (wpn != nullptr && wpn->WpnCanShoot() && wpn->GetAimFactor() > 0.001f)
				{
					Fvector3 aim_offset = AimOffset;
					aim_offset.mul(wpn->GetAimFactor());
					curr_light_offset.add(aim_offset);
					curr_omni_offset.add(aim_offset);
				}
			}

			kin->LL_GetTransform(lightBoneId).transform_tiny(lightPos, curr_light_offset);
			kin->LL_GetTransform(lightBoneId).transform_tiny(omniPos, curr_omni_offset);

			if (IsLightDirByBone)
			{
				lightDirBoneId = kin->LL_BoneID(LightDirBoneName);
				kin->LL_GetTransform(lightDirBoneId).transform_tiny(lightDir, LightOffset);
				lightDir = lightDir.sub(lightPos).normalize();
			}
		}
		else
		{
			xform = item->object().XFORM();
			kin = item->object().Visual()->dcast_PKinematics();
			lightBoneId = kin->LL_BoneID(LightBone);
			kin->LL_GetTransform(lightBoneId).transform_tiny(lightPos, LightWorldOffset);
			kin->LL_GetTransform(lightBoneId).transform_tiny(omniPos, OmniWorldOffset);

			if (IsLightDirByBone)
			{
				lightDirBoneId = kin->LL_BoneID(LightDirBoneName);
				kin->LL_GetTransform(lightDirBoneId).transform_tiny(lightDir, LightWorldOffset);
				lightDir = lightDir.sub(lightPos).normalize();
			}
		}

		xform.transform_tiny(lightPos);
		xform.transform_tiny(omniPos);
		xform.transform_dir(lightDir);

		if (isHudMode)
		{
			Device.transform_hud2world(lightPos, lightDir);
			Device.transform_hud2world(omniPos);
		}

		Fvector::generate_orthonormal_basis_normalized(lightDir, up, right);

		OmniLight->set_hud_mode(false);
		OmniLight->set_position(omniPos);

		RenderLight->set_hud_mode(false);
		RenderLight->set_position(lightPos);
		RenderLight->set_rotation(lightDir, right);

		RenderLight->set_ignore_object(item->object().H_Root());
		if (isHudMode)
		{
			Fobb obb;
			Fmatrix trans = item->HudItemData()->m_item_transform;
			obb.m_rotate.i.set(trans.i);
			obb.m_rotate.j.set(trans.j);
			obb.m_rotate.k.set(trans.k);
			obb.m_halfsize.set(0.005f, 0.005f, 0.005f);

#ifdef DEBUG_DRAW
			if (forceTPDraw)
			{
				Fvector posDebug = lightPos;
				obb.m_translate.set(posDebug);
				obb.xform_full(trans);
				Level().debug_renderer().draw_obb(trans, color_xrgb(255, 0, 0));
			}
#endif
		}
	}

	RenderLight->set_active(IsRenderLight);
	OmniLight->set_active(false);
}

void THudLightTorch::SwitchTorch(bool& saved_status, bool status, bool forced)
{
	if (!forced && status == saved_status)
	{
		return;
	}

	saved_status = status;

	SwitchTorchlight(status);
}

void THudLightTorch::UpdateTorch(CHudItemObject* item, bool& saved_status)
{
	SwitchTorch(saved_status, saved_status, true);

	bool is_broken = false;
	const float current_condition = item->GetCondition();

	const THudLightTorch::breaking_params& BreakingParams = this->BreakingParams;

	if (current_condition < BreakingParams.end_condition)
	{
		is_broken = true;
	}
	else if (current_condition < BreakingParams.start_condition)
	{
		is_broken = (::Random.randF(0.0f, 1.0f) < BreakingParams.start_probability +
			(BreakingParams.start_condition - current_condition) *
			(1.0f - BreakingParams.start_probability) /
			(BreakingParams.start_condition - BreakingParams.end_condition));
	}

	if (is_broken)
	{
		SwitchTorchlight(false);
	}

	auto SetVisible = [&](IKinematics* kin, const shared_str& bone_name, bool status)
	{
		if (kin != nullptr)
		{
			u16 bone_id = kin->LL_BoneID(bone_name);
			if (bone_id != BI_NONE)
			{
				kin->LL_SetBoneVisible(bone_id, status, false);
			}
		}
	};

	attachable_hud_item* HID = item->HudItemData();
	IKinematics* hud_kin = HID != nullptr ? HID->m_model : nullptr;
	IKinematics* world_kin = item->Visual() != nullptr ? PKinematics(item->Visual()) : nullptr;

	for (const shared_str& bone : ConeBones)
	{
		SetVisible(hud_kin, bone, GetTorchActive());
		SetVisible(world_kin, bone, GetTorchActive());
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern float g_fov;

namespace
{
	int g_WeaponLaserColorMode = 0; // 0 = weapon config (LTX), 1 = red 635nm, 2 = green 520nm, 3 = blue 450nm, 4 = yellow
	float g_WeaponLaserBrightness = 1.0f;
	float g_WeaponLaserSize = 1.0f;
	int g_WeaponLaserBeamMode = 1; // 0 = off, 1 = auto (night/fog/rain/dark interiors), 2 = always
	float g_WeaponLaserBeamIntensity = 1.0f;
	int g_WeaponLaserAimMode = 0; // 0 = point of impact (g_fireParams), 1 = weapon axis (live HUD barrel, always on while enabled)

	constexpr const char* kDefaultLaserSpotTexture = "internal\\internal_light_laser_tactical_r2";

	// BEGIN_LASER_BEAM_PURE_MATH
	float LaserSmoothstep(float edge0, float edge1, float x)
	{
		if (std::abs(edge1 - edge0) < 1e-6f)
		{
			return (x >= edge1) ? 1.0f : 0.0f;
		}
		const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	struct SLaserBeamEnvInput
	{
		float sunLum = 0.0f;
		float hemiLum = 0.0f;
		float ambientLum = 0.0f;
		float rainDensity = 0.0f;
		float fogDistance = 350.0f;
		float fogDensity = 0.9f;
		float visDistance = 1.0f;
		float actorHemi = 0.08f;
		bool isIndoorRender = false;
	};

	struct SLaserBeamEnvResult
	{
		float autoVisibility = 0.0f;
		float dayGain = 1.0f;
		float atmospheric = 0.0f;
	};

	SLaserBeamEnvResult EvaluateLaserBeamEnvironment(const SLaserBeamEnvInput& in)
	{
		SLaserBeamEnvResult out;

		const float outdoorLum = std::max(0.0f, in.sunLum) + std::max(0.0f, in.hemiLum) + std::max(0.0f, in.ambientLum);
		const float nightFactor = in.isIndoorRender ? 0.0f : LaserSmoothstep(0.42f, 0.08f, outdoorLum);

		// In R4 CROS_impl scales open-sky hemi by ps_r2_dhemi_sky_scale (default 0.08).
		// Open sky sits near ~0.08, shaded eaves near ~0.04, dark interiors/underground drop below ~0.025.
		const float hemiDark = LaserSmoothstep(0.038f, 0.010f, std::max(0.0f, in.actorHemi));
		const float indoorFactor = in.isIndoorRender ? std::max(hemiDark, 0.65f) : hemiDark;

		// X-Ray weathers commonly keep fog_density at 0.90 even in clear skies and control fog thickness via fog_distance.
		const float safeVis = std::max(in.visDistance, 0.25f);
		const float rawFogDist = std::max(1.0f, in.fogDistance / safeVis);
		const float effectiveFogSpan = rawFogDist / std::max(in.fogDensity, 0.05f);
		const float fogFactor = in.isIndoorRender ? 0.0f : LaserSmoothstep(230.0f, 65.0f, effectiveFogSpan);

		const float rainExposure = in.isIndoorRender ? 0.0f : std::clamp(in.actorHemi / 0.05f, 0.20f, 1.0f);
		const float rainFactor = LaserSmoothstep(0.08f, 0.75f, std::clamp(in.rainDensity, 0.0f, 1.0f)) * rainExposure;

		const float cNight = nightFactor * 0.85f;
		const float cIndoor = indoorFactor * 0.85f;
		const float cFog = fogFactor * 0.90f;
		const float cRain = rainFactor * 0.60f;
		out.autoVisibility = 1.0f - (1.0f - cNight) * (1.0f - cIndoor) * (1.0f - cFog) * (1.0f - cRain);
		out.atmospheric = std::max(fogFactor, rainFactor);

		// Compensate R4 HDR auto-exposure so default intensity=1.0 stays subtly readable both in dark night
		// (scene HDR ~0.005..0.03) and in daytime fog/rain/always-on mode (scene HDR ~0.3..1.5).
		const float effectiveSceneLum = in.isIndoorRender
			? std::max(0.0f, in.actorHemi) * 4.0f
			: outdoorLum * std::clamp(in.actorHemi / 0.06f, 0.15f, 1.0f);
		out.dayGain = std::clamp(1.0f + effectiveSceneLum * 4.8f, 1.0f, 7.5f);

		return out;
	}

	float ResolveLaserBeamModeVisibility(int beamMode, float autoVisibility)
	{
		if (beamMode <= 0)
		{
			return 0.0f;
		}
		if (beamMode >= 2)
		{
			return std::max(0.65f, std::clamp(autoVisibility, 0.0f, 1.0f));
		}
		return std::clamp(autoVisibility, 0.0f, 1.0f);
	}

	float ComputeLaserBeamAimZoomFade(float aimFactor, bool hasScopeTexture, float zoomScale)
	{
		const float clampedAim = std::clamp(aimFactor, 0.0f, 1.0f);
		const float scope2DFade = hasScopeTexture ? LaserSmoothstep(0.65f, 0.15f, clampedAim) : 1.0f;
		const float adsFactor = 1.0f - 0.35f * clampedAim;
		const float zoomFade = LaserSmoothstep(2.4f, 1.45f, std::max(1.0f, zoomScale));
		return scope2DFade * adsFactor * zoomFade;
	}

	float ComputeLaserBeamDustModulation(float wx, float wy, float wz, float timeSec, float atmospheric)
	{
		const float p1 = std::sin(wx * 2.7f + wz * 2.1f + timeSec * 0.55f)
			* std::cos(wy * 3.1f - wx * 1.6f - timeSec * 0.42f);
		const float p2 = std::sin(wx * 6.3f - wy * 5.2f + wz * 4.9f - timeSec * 0.95f);
		const float wave = 0.65f * p1 + 0.35f * p2;
		const float amp = 0.24f + 0.10f * std::clamp(atmospheric, 0.0f, 1.0f);
		return std::clamp(1.0f + amp * wave, 0.62f, 1.38f);
	}

	void ComputeLaserBeamRenderWidthAndCoverage(
		float physHalfW,
		float camDist,
		float projY,
		float targetHeight,
		float minHalfPixels,
		float& outRenderHalfW,
		float& outCoverage)
	{
		const float safePhys = std::max(physHalfW, 1e-5f);
		const float safeDist = std::max(camDist, 0.05f);
		const float safeProjY = std::max(std::abs(projY), 0.25f);
		const float safeHeight = std::max(targetHeight, 240.0f);
		const float minScreenHalfW = safeDist * (minHalfPixels / (0.5f * safeHeight * safeProjY));
		if (safePhys < minScreenHalfW)
		{
			outRenderHalfW = minScreenHalfW;
			outCoverage = safePhys / minScreenHalfW;
		}
		else
		{
			outRenderHalfW = safePhys;
			outCoverage = 1.0f;
		}
	}

	float ComputeLaserBeamVertexScaleForLinear(float targetLinear)
	{
		if (targetLinear <= 1e-6f)
		{
			return 0.0f;
		}
		// effects\\bullet_tracer uses stub_srgb: out.rgb = pow(tex.rgb * vColor.rgb * 0.6f, 2.2f).
		return std::clamp(std::pow(targetLinear, 1.0f / 2.2f) / 0.6f, 0.0f, 1.0f);
	}

	bool ComputeLaserBeamRibbonRightAxis(
		float beamDirX, float beamDirY, float beamDirZ,
		float toCamX, float toCamY, float toCamZ,
		float& outRightX, float& outRightY, float& outRightZ)
	{
		// cross(toCam, beamDir) yields a camera-facing ribbon right vector so that strips
		// (prevL, curL, curC) and (prevC, curC, curR) have CW front-facing screen winding
		// in Direct3D LH clip space (never back-face culled).
		const float rx = toCamY * beamDirZ - toCamZ * beamDirY;
		const float ry = toCamZ * beamDirX - toCamX * beamDirZ;
		const float rz = toCamX * beamDirY - toCamY * beamDirX;
		const float lenSq = rx * rx + ry * ry + rz * rz;
		if (lenSq <= 1e-8f)
		{
			outRightX = 0.0f;
			outRightY = 0.0f;
			outRightZ = 0.0f;
			return false;
		}
		const float invLen = 1.0f / std::sqrt(lenSq);
		outRightX = rx * invLen;
		outRightY = ry * invLen;
		outRightZ = rz * invLen;
		return true;
	}

	bool ShouldSuppressHudLaser(int aimMode, bool weaponReady, bool actorSprinting, bool actorSafemode)
	{
		if (aimMode >= 1)
		{
			return false;
		}
		return !weaponReady || actorSprinting || actorSafemode;
	}

	float ComputeLaserHipVisibleDiameter(float hitDist, float sizeScale)
	{
		const float safeDist = std::max(hitDist, 0.05f);
		const float safeScale = std::clamp(sizeScale, 0.5f, 2.5f);
		return std::clamp(0.010f + safeDist * 0.0030f, 0.010f, 0.30f) * safeScale;
	}

	bool ClipLaserBeamSegmentToNearPlane(
		float startCamZ,
		float endCamZ,
		float dirCamZ,
		float rawLen,
		float minCamZ,
		float minBeamLen,
		bool terminatesAtDot,
		float& outStartT,
		float& outEndT,
		bool& outTerminatesAtDot)
	{
		if (rawLen < minBeamLen)
		{
			return false;
		}

		float t0 = 0.0f;
		float t1 = rawLen;
		bool term = terminatesAtDot;

		if (startCamZ < minCamZ)
		{
			if (dirCamZ <= 0.05f)
			{
				return false;
			}
			t0 = (minCamZ - startCamZ) / dirCamZ;
		}

		if (endCamZ < minCamZ)
		{
			if (dirCamZ >= -0.01f)
			{
				return false;
			}
			const float clipEnd = (minCamZ - startCamZ) / dirCamZ;
			if (clipEnd < t1)
			{
				t1 = clipEnd;
				term = false;
			}
		}

		if (t0 < 0.0f)
		{
			t0 = 0.0f;
		}
		if (t1 > rawLen)
		{
			t1 = rawLen;
		}
		if (t1 - t0 < minBeamLen)
		{
			return false;
		}

		outStartT = t0;
		outEndT = t1;
		outTerminatesAtDot = term;
		return true;
	}

	struct SLaserVec3
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 1.0f;
	};

	SLaserVec3 NormalizeLaserVec3(const SLaserVec3& v, const SLaserVec3& fallback = { 0.0f, 0.0f, 1.0f })
	{
		const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
		if (!std::isfinite(lenSq) || lenSq <= 1e-10f)
		{
			return fallback;
		}
		const float invLen = 1.0f / std::sqrt(lenSq);
		return { v.x * invLen, v.y * invLen, v.z * invLen };
	}

	// Matches CWeapon::UpdateHudAdditonal rotation order: rotateX -> mulA_43(rotateY) -> mulA_43(rotateZ)
	// (applies X, then Y, then Z to a HUD camera-space direction vector).
	SLaserVec3 ApplyHudAimRotation(const SLaserVec3& rotRad, const SLaserVec3& dirCam)
	{
		const float cx = std::cos(rotRad.x);
		const float sx = std::sin(rotRad.x);
		const float cy = std::cos(rotRad.y);
		const float sy = std::sin(rotRad.y);
		const float cz = std::cos(rotRad.z);
		const float sz = std::sin(rotRad.z);

		const float x1 = dirCam.x;
		const float y1 = dirCam.y * cx - dirCam.z * sx;
		const float z1 = dirCam.y * sx + dirCam.z * cx;

		const float x2 = x1 * cy + z1 * sy;
		const float y2 = y1;
		const float z2 = -x1 * sy + z1 * cy;

		const float x3 = x2 * cz - y2 * sz;
		const float y3 = x2 * sz + y2 * cz;
		const float z3 = z2;

		return NormalizeLaserVec3({ x3, y3, z3 });
	}

	// Exact inverse of ApplyHudAimRotation: rotateZ(-z) -> rotateY(-y) -> rotateX(-x).
	SLaserVec3 ApplyInverseHudAimRotation(const SLaserVec3& rotRad, const SLaserVec3& dirCam)
	{
		const float cx = std::cos(-rotRad.x);
		const float sx = std::sin(-rotRad.x);
		const float cy = std::cos(-rotRad.y);
		const float sy = std::sin(-rotRad.y);
		const float cz = std::cos(-rotRad.z);
		const float sz = std::sin(-rotRad.z);

		const float x1 = dirCam.x * cz - dirCam.y * sz;
		const float y1 = dirCam.x * sz + dirCam.y * cz;
		const float z1 = dirCam.z;

		const float x2 = x1 * cy + z1 * sy;
		const float y2 = y1;
		const float z2 = -x1 * sy + z1 * cy;

		const float x3 = x2;
		const float y3 = y2 * cx - z2 * sx;
		const float z3 = y2 * sx + z2 * cx;

		return NormalizeLaserVec3({ x3, y3, z3 });
	}

	// Minimal 3D arc rotation mapping restCam -> (0, 0, 1) applied to boneDirCam.
	// Zeroes out static STCoP HUD/optic model tilt while preserving 100% of dynamic sway,
	// breathing, recoil, mouse inertia, wall collision, reload, sprint, and safemode motion.
	SLaserVec3 ApplyLaserZeroRotation(const SLaserVec3& boneDirCam, const SLaserVec3& restCam)
	{
		const SLaserVec3 v = NormalizeLaserVec3(boneDirCam);
		const SLaserVec3 a = NormalizeLaserVec3(restCam);
		if (a.z < 0.96f || a.z >= 0.9999995f)
		{
			return v;
		}

		const float qw = std::sqrt(0.5f * (1.0f + a.z));
		const float inv2Qw = 0.5f / qw;
		const float qx = a.y * inv2Qw;
		const float qy = -a.x * inv2Qw;

		const float tx = 2.0f * (qy * v.z);
		const float ty = 2.0f * (-qx * v.z);
		const float tz = 2.0f * (qx * v.y - qy * v.x);

		const float rx = v.x + qw * tx + (qy * tz);
		const float ry = v.y + qw * ty + (-qx * tz);
		const float rz = v.z + qw * tz + (qx * ty - qy * tx);

		return NormalizeLaserVec3({ rx, ry, rz }, v);
	}

	float ComputeLaserAimBlend(
		const SLaserVec3& savedRot,
		const SLaserVec3& aimRot,
		float aimFactor,
		bool isSafemode,
		bool isGrenadeMode)
	{
		if (isSafemode || isGrenadeMode)
		{
			return 0.0f;
		}
		const float aimSq = aimRot.x * aimRot.x + aimRot.y * aimRot.y + aimRot.z * aimRot.z;
		if (aimSq > 1e-6f)
		{
			const float proj = (savedRot.x * aimRot.x + savedRot.y * aimRot.y + savedRot.z * aimRot.z) / aimSq;
			return std::clamp(proj, 0.0f, 1.0f);
		}
		return std::clamp(aimFactor, 0.0f, 1.0f);
	}

	struct SLaserZeroCalibState
	{
		bool hasHipRest = false;
		SLaserVec3 hipRestCam = { 0.0f, 0.0f, 1.0f };
		float hipCalmSec = 0.0f;

		bool hasAimRest = false;
		SLaserVec3 aimRestCam = { 0.0f, 0.0f, 1.0f };
		float aimCalmSec = 0.0f;

		float camStillSec = 0.0f;

		void ResetAll()
		{
			hasHipRest = false;
			hipRestCam = { 0.0f, 0.0f, 1.0f };
			hipCalmSec = 0.0f;
			hasAimRest = false;
			aimRestCam = { 0.0f, 0.0f, 1.0f };
			aimCalmSec = 0.0f;
			camStillSec = 0.0f;
		}

		void ResetAim()
		{
			hasAimRest = false;
			aimRestCam = { 0.0f, 0.0f, 1.0f };
			aimCalmSec = 0.0f;
		}
	};

	struct SLaserZeroSampleInput
	{
		SLaserVec3 boneDirCam = { 0.0f, 0.0f, 1.0f };
		SLaserVec3 aimRot = { 0.0f, 0.0f, 0.0f };
		SLaserVec3 savedRot = { 0.0f, 0.0f, 0.0f };
		float aimFactor = 0.0f;
		float dt = 0.0f;
		bool isIdle = false;
		bool isIdleSettled = false;
		bool isStillNow = false;
		bool isHipSettled = false;
		bool isAimSettled = false;
		bool isSafemode = false;
		bool isGrenadeMode = false;
	};

	bool StepLaserZeroCalibration(
		SLaserZeroCalibState& state,
		const SLaserZeroSampleInput& in,
		SLaserVec3& outRestCam)
	{
		const float safeDt = std::clamp(in.dt, 0.0f, 0.25f);
		const SLaserVec3 sample = NormalizeLaserVec3(in.boneDirCam);

		if (in.isIdle && in.isStillNow)
		{
			state.camStillSec = std::min(state.camStillSec + safeDt, 10.0f);
		}
		else
		{
			state.camStillSec = 0.0f;
		}

		constexpr float kMinStillBeforeSampleSec = 0.15f;
		constexpr float kMaxCalibAccumSec = 4.0f;
		constexpr float kPredictedSeedSec = 0.15f;

		if (in.isIdleSettled && state.camStillSec >= kMinStillBeforeSampleSec && sample.z >= 0.96f)
		{
			auto accumulateRest = [&](bool& hasRest, SLaserVec3& restCam, float& calmSec, bool hasSeed, const SLaserVec3& seedDir)
			{
				if (calmSec >= kMaxCalibAccumSec)
				{
					return;
				}
				if (!hasRest)
				{
					if (hasSeed && seedDir.z >= 0.96f)
					{
						restCam = seedDir;
						calmSec = kPredictedSeedSec;
						hasRest = true;
					}
					else
					{
						restCam = sample;
						calmSec = std::max(safeDt, 0.01f);
						hasRest = true;
						return;
					}
				}
				if (safeDt <= 0.0f)
				{
					return;
				}
				const float nextSec = std::min(calmSec + safeDt, kMaxCalibAccumSec);
				const float w = std::clamp(safeDt / std::max(nextSec, 1e-4f), 0.0f, 1.0f);
				restCam = NormalizeLaserVec3({
					restCam.x + (sample.x - restCam.x) * w,
					restCam.y + (sample.y - restCam.y) * w,
					restCam.z + (sample.z - restCam.z) * w,
				});
				calmSec = nextSec;
			};

			if (in.isHipSettled)
			{
				const SLaserVec3 seedHip = state.hasAimRest
					? ApplyInverseHudAimRotation(in.aimRot, state.aimRestCam)
					: SLaserVec3{ 0.0f, 0.0f, 1.0f };
				accumulateRest(state.hasHipRest, state.hipRestCam, state.hipCalmSec, state.hasAimRest, seedHip);
			}
			else if (in.isAimSettled)
			{
				const SLaserVec3 seedAim = state.hasHipRest
					? ApplyHudAimRotation(in.aimRot, state.hipRestCam)
					: SLaserVec3{ 0.0f, 0.0f, 1.0f };
				accumulateRest(state.hasAimRest, state.aimRestCam, state.aimCalmSec, state.hasHipRest, seedAim);
			}
		}

		if (!state.hasHipRest && !state.hasAimRest)
		{
			outRestCam = { 0.0f, 0.0f, 1.0f };
			return false;
		}

		const SLaserVec3 effHip = state.hasHipRest
			? state.hipRestCam
			: ApplyInverseHudAimRotation(in.aimRot, state.aimRestCam);
		const SLaserVec3 effAim = state.hasAimRest
			? state.aimRestCam
			: ApplyHudAimRotation(in.aimRot, state.hipRestCam);

		const float aimBlend = ComputeLaserAimBlend(in.savedRot, in.aimRot, in.aimFactor, in.isSafemode, in.isGrenadeMode);
		outRestCam = NormalizeLaserVec3({
			effHip.x + (effAim.x - effHip.x) * aimBlend,
			effHip.y + (effAim.y - effHip.y) * aimBlend,
			effHip.z + (effAim.z - effHip.z) * aimBlend,
		});
		return outRestCam.z >= 0.96f;
	}
	// END_LASER_BEAM_PURE_MATH

	struct SActiveLaserDot
	{
		const void* owner = nullptr;
		const attachable_hud_item* hudItem = nullptr;
		u16 hudAttachIdx = u16(-1);
		u16 lightBoneId = BI_NONE;
		u32 frame = 0;
		Fvector hitPos = { 0.0f, 0.0f, 0.0f };
		Fvector rayDir = { 0.0f, 0.0f, 1.0f };
		Fvector lightOffset = { 0.0f, 0.0f, 0.0f };
		Fvector fallbackEmitterWorld = { 0.0f, 0.0f, 0.0f };
		Fvector beamTargetPos = { 0.0f, 0.0f, 0.0f };
		Fvector zeroRestCam = { 0.0f, 0.0f, 1.0f };
		Fvector dotSrgb = { 1.0f, 0.04f, 0.02f };
		float visibleDiam = 0.02f;
		float sizeScale = 1.0f;
		float brightness = 1.0f;
		float aimFactor = 0.0f;
		float beamAutoVis = 0.0f;
		float beamDayGain = 1.0f;
		float beamAtmospheric = 0.0f;
		float laserMaxDist = 80.0f;
		shared_str textureName;
		bool hasDot = false;
		bool hasBeam = false;
		bool hasScopeTexture = false;
		bool useWeaponAxis = false;
		bool hasZeroRest = false;
		bool active = false;
	};

	struct SLaserZeroCache
	{
		const void* owner = nullptr;
		shared_str hudSection;
		u16 lightBoneId = BI_NONE;
		EHudOffsetType aimOffsetIdx = EHudOffsetType::eAim;
		u32 lastFrame = 0;
		SLaserZeroCalibState calib;

		void Reset()
		{
			owner = nullptr;
			hudSection = nullptr;
			lightBoneId = BI_NONE;
			aimOffsetIdx = EHudOffsetType::eAim;
			lastFrame = 0;
			calib.ResetAll();
		}
	};

	float ComputeCameraZoomScale()
	{
		const float currentProjY = std::max(std::abs(Device.mProject._22), 0.25f);
		const float baseFovDeg = std::clamp(g_fov, 40.0f, 120.0f);
		const float baseProjY = 1.0f / std::tan(deg2rad(baseFovDeg) * 0.5f);
		return std::max(1.0f, currentProjY / baseProjY);
	}

	SActiveLaserDot g_ActiveLaserDot;
	SLaserZeroCache g_LaserZeroCache;
	float g_SmoothedBeamAutoVis = -1.0f;
	float g_SmoothedBeamDayGain = -1.0f;
	float g_SmoothedBeamAtmospheric = -1.0f;
	u32 g_SmoothedBeamFrame = 0;
	FactoryPtr<IUIShader>* g_LaserDotMaskShader = nullptr;
	FactoryPtr<IUIShader>* g_LaserDotAddShader = nullptr;
	shared_str g_LaserDotShaderTexture;

	void ClearActiveLaserDotForOwner(const void* owner)
	{
		if (owner == nullptr || g_ActiveLaserDot.owner == owner)
		{
			g_ActiveLaserDot.active = false;
			g_ActiveLaserDot.hasDot = false;
			g_ActiveLaserDot.hasBeam = false;
			g_ActiveLaserDot.useWeaponAxis = false;
			g_ActiveLaserDot.hasZeroRest = false;
			g_ActiveLaserDot.zeroRestCam.set(0.0f, 0.0f, 1.0f);
			g_ActiveLaserDot.owner = nullptr;
			g_ActiveLaserDot.hudItem = nullptr;
		}
	}

	void ClearLaserZeroCacheForOwner(const void* owner)
	{
		if (owner == nullptr || g_LaserZeroCache.owner == owner)
		{
			g_LaserZeroCache.Reset();
		}
	}

	bool TryComputeHudBoneDirCam(
		const attachable_hud_item* hudData,
		u16 lightBoneId,
		Fvector& outDirCam)
	{
		if (hudData == nullptr)
		{
			return false;
		}

		Fvector hudDir = { 0.0f, 0.0f, 1.0f };
		if (hudData->m_model != nullptr &&
			lightBoneId != BI_NONE &&
			lightBoneId < hudData->m_model->LL_BoneCount())
		{
			hudData->m_model->LL_GetTransform(lightBoneId).transform_dir(hudDir);
		}
		hudData->m_item_transform.transform_dir(hudDir);

		const float hudDirLenSq = hudDir.square_magnitude();
		if (!std::isfinite(hudDirLenSq) || hudDirLenSq <= EPS_S)
		{
			return false;
		}
		hudDir.div(std::sqrt(hudDirLenSq));

		outDirCam.set(
			hudDir.dotproduct(Device.vCameraRight),
			hudDir.dotproduct(Device.vCameraTop),
			hudDir.dotproduct(Device.vCameraDirection));
		const float camLenSq = outDirCam.square_magnitude();
		if (!std::isfinite(camLenSq) || camLenSq <= EPS_S)
		{
			return false;
		}
		outDirCam.div(std::sqrt(camLenSq));
		return true;
	}

	bool ComputeHudWeaponAxisRay(
		const attachable_hud_item* hudData,
		u16 lightBoneId,
		const Fvector& lightOffset,
		const Fvector* zeroRestCam,
		CObject* ignoreObj,
		Fvector& outEmitterWorld,
		Fvector& outRayPos,
		Fvector& outRayDir)
	{
		if (hudData == nullptr)
		{
			return false;
		}

		Fvector hudPos = lightOffset;
		Fvector hudDir = { 0.0f, 0.0f, 1.0f };
		if (hudData->m_model != nullptr &&
			lightBoneId != BI_NONE &&
			lightBoneId < hudData->m_model->LL_BoneCount())
		{
			const Fmatrix& boneXform = hudData->m_model->LL_GetTransform(lightBoneId);
			boneXform.transform_tiny(hudPos, lightOffset);
			boneXform.transform_dir(hudDir);
		}
		hudData->m_item_transform.transform_tiny(hudPos);
		hudData->m_item_transform.transform_dir(hudDir);

		const float hudDirLenSq = hudDir.square_magnitude();
		if (!std::isfinite(hudPos.x) || !std::isfinite(hudPos.y) || !std::isfinite(hudPos.z) ||
			!std::isfinite(hudDirLenSq) || hudDirLenSq <= EPS_S)
		{
			return false;
		}
		hudDir.div(std::sqrt(hudDirLenSq));

		if (zeroRestCam != nullptr)
		{
			const SLaserVec3 boneDirCam{
				hudDir.dotproduct(Device.vCameraRight),
				hudDir.dotproduct(Device.vCameraTop),
				hudDir.dotproduct(Device.vCameraDirection),
			};
			const SLaserVec3 restCam{ zeroRestCam->x, zeroRestCam->y, zeroRestCam->z };
			const SLaserVec3 zeroedCam = ApplyLaserZeroRotation(boneDirCam, restCam);
			hudDir.mul(Device.vCameraRight, zeroedCam.x);
			hudDir.mad(Device.vCameraTop, zeroedCam.y);
			hudDir.mad(Device.vCameraDirection, zeroedCam.z);
			hudDir.normalize_safe();
		}

		Fvector worldPos = hudPos;
		Fvector worldDir = hudDir;
		Device.transform_hud2world(worldPos, worldDir);

		const float worldDirLenSq = worldDir.square_magnitude();
		if (!std::isfinite(worldPos.x) || !std::isfinite(worldPos.y) || !std::isfinite(worldPos.z) ||
			!std::isfinite(worldDirLenSq) || worldDirLenSq <= EPS_S)
		{
			return false;
		}
		worldDir.div(std::sqrt(worldDirLenSq));

		outEmitterWorld = worldPos;
		outRayPos = worldPos;
		outRayDir = worldDir;

		// Prevent the ray origin from starting behind a wall when the HUD muzzle clips into close geometry.
		if (g_pGameLevel != nullptr)
		{
			Fvector camToEmitter;
			camToEmitter.sub(worldPos, Device.vCameraPosition);
			const float camToEmitterLen = camToEmitter.magnitude();
			if (camToEmitterLen > 0.02f)
			{
				Fvector camToEmitterDir = camToEmitter;
				camToEmitterDir.div(camToEmitterLen);
				collide::rq_result clipRQ;
				clipRQ.range = camToEmitterLen;
				if (g_pGameLevel->ObjectSpace.RayPick(
					Device.vCameraPosition,
					camToEmitterDir,
					clipRQ.range,
					collide::rqtBoth,
					clipRQ,
					ignoreObj))
				{
					const float safeDist = std::max(0.0f, clipRQ.range - 0.02f);
					outRayPos.mad(Device.vCameraPosition, camToEmitterDir, safeDist);
				}
			}
		}

		return true;
	}

	u32 EncodeLaserBeamColor(const Fvector& dotSrgb, float targetLinear)
	{
		const float vScale = ComputeLaserBeamVertexScaleForLinear(targetLinear);
		if (vScale <= 0.0f)
		{
			return color_rgba(0u, 0u, 0u, 0u);
		}
		const u32 r = static_cast<u32>(std::clamp(std::lround(dotSrgb.x * vScale * 255.0f), 0L, 255L));
		const u32 g = static_cast<u32>(std::clamp(std::lround(dotSrgb.y * vScale * 255.0f), 0L, 255L));
		const u32 b = static_cast<u32>(std::clamp(std::lround(dotSrgb.z * vScale * 255.0f), 0L, 255L));
		return color_rgba(r, g, b, 255u);
	}

	void RenderActiveWeaponLaserBeam(
		const Fvector& emitterPos,
		const Fvector& activeRayDir,
		const Fvector& beamEndPos,
		bool terminatesAtDot,
		float zoomScale)
	{
		if (g_WeaponLaserBeamMode <= 0 || !g_ActiveLaserDot.hasBeam || !(*g_LaserDotAddShader)->inited())
		{
			return;
		}

		const float modeVis = ResolveLaserBeamModeVisibility(g_WeaponLaserBeamMode, g_ActiveLaserDot.beamAutoVis);
		const float aimZoomFade = ComputeLaserBeamAimZoomFade(
			g_ActiveLaserDot.aimFactor,
			g_ActiveLaserDot.hasScopeTexture,
			zoomScale);
		const float intensity = std::clamp(g_WeaponLaserBeamIntensity, 0.1f, 3.0f);
		const float baseCoreLinear = 0.020f * intensity * modeVis * aimZoomFade * g_ActiveLaserDot.beamDayGain;
		if (baseCoreLinear <= 5e-5f)
		{
			return;
		}

		if (!std::isfinite(emitterPos.x) || !std::isfinite(emitterPos.y) || !std::isfinite(emitterPos.z) ||
			!std::isfinite(beamEndPos.x) || !std::isfinite(beamEndPos.y) || !std::isfinite(beamEndPos.z))
		{
			return;
		}

		Fvector beamVec;
		beamVec.sub(beamEndPos, emitterPos);
		const float forwardProj = beamVec.dotproduct(activeRayDir);
		const float rawLen = beamVec.magnitude();
		if (forwardProj < 0.08f || rawLen < 0.08f)
		{
			return;
		}

		Fvector beamDir = beamVec;
		beamDir.div(rawLen);

		// Keep both ends of the ribbon safely ahead of the world near plane (including when the barrel
		// tilts backward during reload/holster in weapon-axis mode); HUD geometry in RDepth clips behind the lens.
		Fvector camToEmitter;
		camToEmitter.sub(emitterPos, Device.vCameraPosition);
		const float emitterCamZ = camToEmitter.dotproduct(Device.vCameraDirection);
		Fvector camToEnd;
		camToEnd.sub(beamEndPos, Device.vCameraPosition);
		const float endCamZ = camToEnd.dotproduct(Device.vCameraDirection);
		const float dirCamZ = beamDir.dotproduct(Device.vCameraDirection);
		const float minCamZ = std::max(Device.fViewportNear, 0.05f) + 0.03f;

		float startT = 0.0f;
		float endT = rawLen;
		bool effectiveTerminatesAtDot = terminatesAtDot;
		if (!ClipLaserBeamSegmentToNearPlane(
			emitterCamZ,
			endCamZ,
			dirCamZ,
			rawLen,
			minCamZ,
			0.06f,
			terminatesAtDot,
			startT,
			endT,
			effectiveTerminatesAtDot))
		{
			return;
		}

		Fvector beamStart;
		beamStart.mad(emitterPos, beamDir, startT);
		const float beamLen = endT - startT;

		// Smoothly attenuate when the muzzle approaches within centimeters of an obstacle.
		const float closeRangeFade = LaserSmoothstep(0.08f, 0.25f, forwardProj);

		// Because beamStart + t*beamDir is a straight line, cross(camPos - P(t), beamDir) = cross(camPos - beamStart, beamDir)
		// is constant along the entire beam, guaranteeing a twist-free CW front-facing ribbon.
		Fvector toCamStart;
		toCamStart.sub(Device.vCameraPosition, beamStart);
		Fvector right;
		if (!ComputeLaserBeamRibbonRightAxis(
			beamDir.x, beamDir.y, beamDir.z,
			toCamStart.x, toCamStart.y, toCamStart.z,
			right.x, right.y, right.z))
		{
			right = Device.vCameraRight;
		}

		constexpr int kMaxSegments = 24;
		const float haloRatio = 0.14f + 0.14f * std::clamp(g_ActiveLaserDot.beamAtmospheric, 0.0f, 1.0f);
		const bool drawHalo = (baseCoreLinear * closeRangeFade * haloRatio) > 8e-5f;
		const int layerCount = drawHalo ? 2 : 1;

		// Each segment uses a 3-vertex cross-section (edge 0 -> center spine -> edge 0 = 2 quads = 12 verts)
		// sampling constant center UV (0.5, 0.5), producing an alias-free (1 - |t|)^2.2 bell profile through stub_srgb.
		const u32 maxSegments = UIRender->VBuffMaxSize() / static_cast<u32>(sizeof(IUIRender::r_vertLIT<12>));
		const int segCount = std::min(kMaxSegments, static_cast<int>(maxSegments / static_cast<u32>(layerCount)));
		if (segCount < 1)
		{
			return;
		}

		const float projY = std::max(std::abs(Device.mProject._22), 0.25f);
		const float viewH = static_cast<float>(std::max<u32>(Device.TargetHeight, 240u));
		constexpr float uvCenter = 0.5f;
		const u32 edgeClr = color_rgba(0u, 0u, 0u, 0u);
		const float timeSec = Device.fTimeGlobal;
		const float fadeInDist = std::min(0.08f, beamLen * 0.22f);
		const float fadeOutDist = effectiveTerminatesAtDot ? std::min(0.10f, beamLen * 0.15f) : (beamLen * 0.35f);

		auto emitRibbonLayer = [&](float baseHalfW, float divergence, float layerEnergyScale, float minHalfPx)
		{
			Fvector prevL, prevC, prevR;
			u32 prevClr = 0u;
			for (int i = 0; i <= segCount; ++i)
			{
				const float u = static_cast<float>(i) / static_cast<float>(segCount);
				// Concentrate segments closer to the weapon where screen space length and dust detail are largest.
				const float s = 0.25f * u + 0.75f * u * u;
				const float dist = s * beamLen;

				Fvector curC;
				curC.mad(beamStart, beamDir, dist);

				const float camDist = Device.vCameraPosition.distance_to(curC);
				const float physHalfW = baseHalfW + dist * divergence;
				float renderHalfW = physHalfW;
				float coverage = 1.0f;
				ComputeLaserBeamRenderWidthAndCoverage(physHalfW, camDist, projY, viewH, minHalfPx, renderHalfW, coverage);

				const float startFade = 0.30f + 0.70f * LaserSmoothstep(0.0f, fadeInDist, dist);
				const float distToEnd = beamLen - dist;
				const float endMin = effectiveTerminatesAtDot ? 0.35f : 0.0f;
				const float endFade = endMin + (1.0f - endMin) * LaserSmoothstep(0.0f, fadeOutDist, distToEnd);
				const float distFade = 1.0f / (1.0f + dist * 0.045f);
				const float dustMod = ComputeLaserBeamDustModulation(curC.x, curC.y, curC.z, timeSec, g_ActiveLaserDot.beamAtmospheric);

				const float segLinear = baseCoreLinear * closeRangeFade * layerEnergyScale * coverage * startFade * endFade * distFade * dustMod;
				const u32 clr = EncodeLaserBeamColor(g_ActiveLaserDot.dotSrgb, segLinear);

				Fvector halfRight = right;
				halfRight.mul(renderHalfW);
				Fvector curL, curR;
				curL.sub(curC, halfRight);
				curR.add(curC, halfRight);

				if (i > 0)
				{
					// Left half-ribbon (edge 0 -> center spine): CW front-facing
					UIRender->PushPoint(prevL.x, prevL.y, prevL.z, edgeClr, uvCenter, uvCenter);
					UIRender->PushPoint(curL.x, curL.y, curL.z, edgeClr, uvCenter, uvCenter);
					UIRender->PushPoint(curC.x, curC.y, curC.z, clr, uvCenter, uvCenter);
					UIRender->PushPoint(prevL.x, prevL.y, prevL.z, edgeClr, uvCenter, uvCenter);
					UIRender->PushPoint(curC.x, curC.y, curC.z, clr, uvCenter, uvCenter);
					UIRender->PushPoint(prevC.x, prevC.y, prevC.z, prevClr, uvCenter, uvCenter);

					// Right half-ribbon (center spine -> edge 0): CW front-facing
					UIRender->PushPoint(prevC.x, prevC.y, prevC.z, prevClr, uvCenter, uvCenter);
					UIRender->PushPoint(curC.x, curC.y, curC.z, clr, uvCenter, uvCenter);
					UIRender->PushPoint(curR.x, curR.y, curR.z, edgeClr, uvCenter, uvCenter);
					UIRender->PushPoint(prevC.x, prevC.y, prevC.z, prevClr, uvCenter, uvCenter);
					UIRender->PushPoint(curR.x, curR.y, curR.z, edgeClr, uvCenter, uvCenter);
					UIRender->PushPoint(prevR.x, prevR.y, prevR.z, edgeClr, uvCenter, uvCenter);
				}

				prevL = curL;
				prevC = curC;
				prevR = curR;
				prevClr = clr;
			}
		};

		UIRender->SetShader(**g_LaserDotAddShader);
		// SetShader applies the shader's rasterizer state (CULL_BACK by default); disable culling after SetShader.
		UIRender->CacheSetCullMode(ERHI_CULLMODE::NONE);
		UIRender->StartPrimitive(static_cast<u32>(layerCount * segCount * 12), IUIRender::ptTriList, IUIRender::pttLIT);
		if (drawHalo)
		{
			emitRibbonLayer(0.0160f, 0.0018f, haloRatio, 4.2f);
		}
		emitRibbonLayer(0.0055f, 0.00055f, 1.0f, 2.4f);
		UIRender->FlushPrimitive();
	}

	const char* ResolveLaserSpotTexture(const char* section)
	{
		if (section == nullptr || section[0] == '\0')
		{
			return kDefaultLaserSpotTexture;
		}
		static const char* s_LastSection = nullptr;
		static shared_str s_LastTexture;
		if (section != s_LastSection)
		{
			s_LastSection = section;
			s_LastTexture = READ_IF_EXISTS(pSettings, r_string, section, "laser_spot_texture", kDefaultLaserSpotTexture);
		}
		return s_LastTexture.size() ? s_LastTexture.c_str() : kDefaultLaserSpotTexture;
	}

	class CCC_WeaponLaserInteger : public CCC_Integer
	{
	public:
		CCC_WeaponLaserInteger(const char* N, int* V, int Min, int Max) : CCC_Integer(N, V, Min, Max) {}
		void Save(IWriter*) override {}
	};

	class CCC_WeaponLaserFloat : public CCC_Float
	{
	public:
		CCC_WeaponLaserFloat(const char* N, float* V, float Min, float Max) : CCC_Float(N, V, Min, Max) {}
		void Execute(const char* args) override
		{
			const float parsed = static_cast<float>(atof(args));
			if (!std::isfinite(parsed) || parsed < min - EPS || parsed > max + EPS)
			{
				InvalidSyntax();
				return;
			}
			*value = std::clamp(parsed, min, max);
		}
		void Save(IWriter*) override {}
	};

	void ResolveWeaponLaserColors(const Fcolor& configColor, Fcolor& outSpotColor, Fvector& outDotSrgb)
	{
		switch (g_WeaponLaserColorMode)
		{
		case 1: // Red 635 nm (L3Harris LA-5B/PEQ)
			outDotSrgb.set(1.0f, 0.04f, 0.02f);
			break;
		case 2: // Green 520 nm
			outDotSrgb.set(0.04f, 1.0f, 0.08f);
			break;
		case 3: // Blue 450 nm
			outDotSrgb.set(0.05f, 0.25f, 1.0f);
			break;
		case 4: // Yellow / amber
			outDotSrgb.set(1.0f, 0.82f, 0.04f);
			break;
		default: // 0 = weapon config (LTX)
		{
			const float r = std::max(0.0f, configColor.r);
			const float g = std::max(0.0f, configColor.g);
			const float b = std::max(0.0f, configColor.b);
			const float peak = std::max(std::max(r, g), std::max(b, 0.001f));
			outDotSrgb.set(r / peak, g / peak, b / peak);
			break;
		}
		}

		const float scale = std::clamp(g_WeaponLaserBrightness, 0.1f, 5.0f);
		// Keep spotlight HDR radiance safely below FP16 max (65504 after GammaToLinear pow 2.2)
		// while delivering strong surface illumination in shadows and at night.
		const float spotIntensity = std::min(110.0f, 42.0f * std::pow(scale, 0.55f));
		outSpotColor.set(
			outDotSrgb.x * spotIntensity,
			outDotSrgb.y * spotIntensity,
			outDotSrgb.z * spotIntensity,
			configColor.a);
	}
} // namespace

void RegisterWeaponLaserConsoleCommands()
{
	CMD4(CCC_WeaponLaserInteger, "wpn_laser_color", &g_WeaponLaserColorMode, 0, 4);
	CMD4(CCC_WeaponLaserFloat, "wpn_laser_brightness", &g_WeaponLaserBrightness, 0.1f, 5.0f);
	CMD4(CCC_WeaponLaserFloat, "wpn_laser_size", &g_WeaponLaserSize, 0.5f, 2.5f);
	CMD4(CCC_WeaponLaserInteger, "wpn_laser_beam", &g_WeaponLaserBeamMode, 0, 2);
	CMD4(CCC_WeaponLaserFloat, "wpn_laser_beam_intensity", &g_WeaponLaserBeamIntensity, 0.1f, 3.0f);
	CMD4(CCC_WeaponLaserInteger, "wpn_laser_mode", &g_WeaponLaserAimMode, 0, 1);
}

void DestroyWeaponLaserDotResources()
{
	g_ActiveLaserDot.active = false;
	g_ActiveLaserDot.hasDot = false;
	g_ActiveLaserDot.hasBeam = false;
	g_ActiveLaserDot.useWeaponAxis = false;
	g_ActiveLaserDot.hasZeroRest = false;
	g_ActiveLaserDot.zeroRestCam.set(0.0f, 0.0f, 1.0f);
	g_ActiveLaserDot.owner = nullptr;
	g_ActiveLaserDot.hudItem = nullptr;
	g_LaserZeroCache.Reset();
	g_SmoothedBeamAutoVis = -1.0f;
	g_SmoothedBeamDayGain = -1.0f;
	g_SmoothedBeamAtmospheric = -1.0f;
	g_SmoothedBeamFrame = 0;
	g_LaserDotShaderTexture = nullptr;

	if (g_LaserDotMaskShader != nullptr)
	{
		if ((*g_LaserDotMaskShader)->inited())
		{
			(*g_LaserDotMaskShader)->destroy();
		}
		xr_delete(g_LaserDotMaskShader);
	}

	if (g_LaserDotAddShader != nullptr)
	{
		if ((*g_LaserDotAddShader)->inited())
		{
			(*g_LaserDotAddShader)->destroy();
		}
		xr_delete(g_LaserDotAddShader);
	}
}

void RenderActiveWeaponLaserDot()
{
	if (!g_ActiveLaserDot.active || g_pGameLevel == nullptr || g_pGamePersistent == nullptr)
	{
		return;
	}

	if (g_pGamePersistent->m_pMainMenu != nullptr && g_pGamePersistent->m_pMainMenu->IsActive())
	{
		return;
	}

	if (Device.dwFrame > g_ActiveLaserDot.frame + 1)
	{
		ClearActiveLaserDotForOwner(nullptr);
		return;
	}

	if (UIRender == nullptr || RenderFactory == nullptr)
	{
		return;
	}

	// Recompute emitter (and in weapon-axis mode the live barrel ray & hit point) in the render pass
	// so the dot and 3D beam remain coaxially locked to the HUD weapon with zero 1-frame lag.
	Fvector effectiveEmitter = g_ActiveLaserDot.fallbackEmitterWorld;
	Fvector effectiveHitPos = g_ActiveLaserDot.hitPos;
	Fvector effectiveBeamTargetPos = g_ActiveLaserDot.beamTargetPos;
	Fvector effectiveRayDir = g_ActiveLaserDot.rayDir;
	float effectiveHipVisibleDiam = g_ActiveLaserDot.visibleDiam;
	bool effectiveHasDot = g_ActiveLaserDot.hasDot;

	if (g_player_hud != nullptr && g_ActiveLaserDot.hudAttachIdx < 2)
	{
		attachable_hud_item* liveHud = g_player_hud->attached_item(g_ActiveLaserDot.hudAttachIdx);
		if (liveHud != nullptr && liveHud == g_ActiveLaserDot.hudItem)
		{
			if (g_ActiveLaserDot.useWeaponAxis)
			{
				CObject* ignoreObj = (liveHud->m_parent_hud_item != nullptr)
					? liveHud->m_parent_hud_item->object().H_Root()
					: nullptr;
				Fvector liveEmitter, liveRayPos, liveRayDir;
				const Fvector* liveZeroRest = g_ActiveLaserDot.hasZeroRest ? &g_ActiveLaserDot.zeroRestCam : nullptr;
				if (ComputeHudWeaponAxisRay(
					liveHud,
					g_ActiveLaserDot.lightBoneId,
					g_ActiveLaserDot.lightOffset,
					liveZeroRest,
					ignoreObj,
					liveEmitter,
					liveRayPos,
					liveRayDir))
				{
					effectiveEmitter = liveEmitter;
					effectiveRayDir = liveRayDir;

					const float maxDist = std::max(g_ActiveLaserDot.laserMaxDist, 1.0f);
					collide::rq_result liveRQ;
					liveRQ.range = maxDist;
					const bool liveHasHit = !!g_pGameLevel->ObjectSpace.RayPick(
						liveRayPos,
						liveRayDir,
						liveRQ.range,
						collide::rqtBoth,
						liveRQ,
						ignoreObj);
					effectiveHasDot = liveHasHit;
					if (liveHasHit)
					{
						const float liveHitDist = std::max(liveRQ.range, 0.001f);
						effectiveHitPos.mad(liveRayPos, liveRayDir, liveHitDist);
						effectiveBeamTargetPos = effectiveHitPos;
						const float sizeDist = std::max(Device.vCameraPosition.distance_to(effectiveHitPos), 0.05f);
						effectiveHipVisibleDiam = ComputeLaserHipVisibleDiameter(sizeDist, g_ActiveLaserDot.sizeScale);
					}
					else
					{
						effectiveBeamTargetPos.mad(liveRayPos, liveRayDir, maxDist);
					}
				}
			}
			else
			{
				Fvector localPos = g_ActiveLaserDot.lightOffset;
				if (liveHud->m_model != nullptr &&
					g_ActiveLaserDot.lightBoneId != BI_NONE &&
					g_ActiveLaserDot.lightBoneId < liveHud->m_model->LL_BoneCount())
				{
					liveHud->m_model->LL_GetTransform(g_ActiveLaserDot.lightBoneId).transform_tiny(localPos, g_ActiveLaserDot.lightOffset);
				}
				liveHud->m_item_transform.transform_tiny(localPos);
				Device.transform_hud2world(localPos);
				effectiveEmitter = localPos;
			}
		}
	}

	if (!effectiveHasDot && (!g_ActiveLaserDot.hasBeam || g_WeaponLaserBeamMode <= 0))
	{
		return;
	}

	const char* texName = g_ActiveLaserDot.textureName.size() ? g_ActiveLaserDot.textureName.c_str() : kDefaultLaserSpotTexture;

	if (g_LaserDotMaskShader == nullptr)
	{
		g_LaserDotMaskShader = new FactoryPtr<IUIShader>();
	}
	if (g_LaserDotAddShader == nullptr)
	{
		g_LaserDotAddShader = new FactoryPtr<IUIShader>();
	}

	if (!(*g_LaserDotMaskShader)->inited() || !(*g_LaserDotAddShader)->inited() || g_LaserDotShaderTexture != texName)
	{
		(*g_LaserDotMaskShader)->create("friendly_indicator", texName);
		(*g_LaserDotAddShader)->create("effects\\bullet_tracer", texName);
		g_LaserDotShaderTexture = texName;
	}

	const bool isDefaultTex = (xr_stricmp(texName, kDefaultLaserSpotTexture) == 0);
	const float zoomScale = ComputeCameraZoomScale();

	UIRender->CacheSetXformWorld(Fidentity);
	UIRender->CacheSetView(Device.mView);
	UIRender->CacheSetProject(Device.mProject);

	bool drawDot = false;
	Fvector drawPos = effectiveBeamTargetPos;
	float quadHalf = 0.0f;
	const float uvMin = isDefaultTex ? (118.0f / 256.0f) : 0.0f;
	const float uvMax = isDefaultTex ? (138.0f / 256.0f) : 1.0f;

	if (effectiveHasDot)
	{
		Fvector toCam;
		toCam.sub(Device.vCameraPosition, effectiveHitPos);
		const float camDist = toCam.magnitude();
		Fvector camToHit;
		camToHit.sub(effectiveHitPos, Device.vCameraPosition);
		const float hitCamZ = camToHit.dotproduct(Device.vCameraDirection);
		if (camDist >= 0.08f && hitCamZ > 0.02f)
		{
			toCam.div(camDist);
			// Compensate for optical / ADS FOV zoom so looking through a magnified scope keeps the dot crisp.
			const float effectiveDiam = std::max(0.002f * g_ActiveLaserDot.sizeScale, effectiveHipVisibleDiam / zoomScale);
			// In internal_light_laser_tactical_r2.dds the visible dot (~12 px) spans 60% of the [118..138] (20 px) crop.
			const float quadHalfAtHit = effectiveDiam * (isDefaultTex ? (0.5f / 0.60f) : 0.5f);
			// Pull slightly toward the camera so the quad clears oblique surfaces without depth clipping
			// while still being occluded cleanly by nearer walls, foliage, and HUD weapon geometry.
			const float bias = std::min(camDist * 0.15f, std::max(0.012f, quadHalfAtHit * 1.15f));
			const float drawDist = std::max(camDist - bias, 0.05f);
			quadHalf = quadHalfAtHit * (drawDist / camDist);
			drawPos.mad(effectiveHitPos, toCam, bias);
			drawDot = true;
		}
	}

	// Draw the optional subtle 3D beam first so Pass 1 of the laser dot preserves a crisp core at the tip.
	const Fvector& beamEndPos = drawDot ? drawPos : effectiveBeamTargetPos;
	RenderActiveWeaponLaserBeam(effectiveEmitter, effectiveRayDir, beamEndPos, drawDot, zoomScale);

	if (!drawDot)
	{
		return;
	}

	Fvector vr = Device.vCameraRight;
	vr.mul(quadHalf);
	Fvector vt = Device.vCameraTop;
	vt.mul(quadHalf);

	Fvector pTL, pTR, pBL, pBR;
	pTL.sub(vt, vr);
	pTL.add(drawPos);
	pTR.add(vt, vr);
	pTR.add(drawPos);
	pBL.sub(drawPos, vt);
	pBL.sub(vr);
	pBR.sub(drawPos, vt);
	pBR.add(vr);

	const float b = std::clamp(g_ActiveLaserDot.brightness, 0.1f, 5.0f);
	const float maskAlphaF = std::clamp(0.30f + 0.68f * (b / (0.22f + b)), 0.30f, 0.96f);
	const u32 colR = static_cast<u32>(std::clamp(std::lround(g_ActiveLaserDot.dotSrgb.x * 255.0f), 0L, 255L));
	const u32 colG = static_cast<u32>(std::clamp(std::lround(g_ActiveLaserDot.dotSrgb.y * 255.0f), 0L, 255L));
	const u32 colB = static_cast<u32>(std::clamp(std::lround(g_ActiveLaserDot.dotSrgb.z * 255.0f), 0L, 255L));
	const u32 maskA = static_cast<u32>(std::clamp(std::lround(maskAlphaF * 255.0f), 0L, 255L));
	const u32 maskColor = color_rgba(colR, colG, colB, maskA);
	const u32 addColor = color_rgba(colR, colG, colB, 255u);

	auto emitQuad = [&](u32 clr)
	{
		UIRender->PushPoint(pBL.x, pBL.y, pBL.z, clr, uvMin, uvMax);
		UIRender->PushPoint(pTL.x, pTL.y, pTL.z, clr, uvMin, uvMin);
		UIRender->PushPoint(pTR.x, pTR.y, pTR.z, clr, uvMax, uvMin);
		UIRender->PushPoint(pBL.x, pBL.y, pBL.z, clr, uvMin, uvMax);
		UIRender->PushPoint(pTR.x, pTR.y, pTR.z, clr, uvMax, uvMin);
		UIRender->PushPoint(pBR.x, pBR.y, pBR.z, clr, uvMax, uvMax);
	};

	// Pass 1: Z-tested alpha-blended mask so the dot stays saturated even on bright sunlit concrete.
	if ((*g_LaserDotMaskShader)->inited())
	{
		UIRender->SetShader(**g_LaserDotMaskShader);
		UIRender->CacheSetCullMode(ERHI_CULLMODE::NONE);
		UIRender->StartPrimitive(6, IUIRender::ptTriList, IUIRender::pttLIT);
		emitQuad(maskColor);
		UIRender->FlushPrimitive();
	}

	// Pass 2: Z-tested additive HDR glow energy (vivid luminous dot in sunlight and darkness alike).
	if ((*g_LaserDotAddShader)->inited())
	{
		const int desiredPasses = std::clamp(static_cast<int>(std::lround(18.0f * b)), 1, 80);
		const u32 maxQuads = std::max<u32>(1u, UIRender->VBuffMaxSize() / static_cast<u32>(sizeof(IUIRender::r_vertLIT<6>)));
		const int safePasses = std::min(desiredPasses, static_cast<int>(maxQuads));
		UIRender->SetShader(**g_LaserDotAddShader);
		UIRender->CacheSetCullMode(ERHI_CULLMODE::NONE);
		UIRender->StartPrimitive(static_cast<u32>(safePasses * 6), IUIRender::ptTriList, IUIRender::pttLIT);
		for (int i = 0; i < safePasses; ++i)
		{
			emitQuad(addColor);
		}
		UIRender->FlushPrimitive();
	}
}

void THudLightLaser::BeginComponent(IECSOwner* O)
{
	THudLightTorch::BeginComponent(O);
	IsLightDirByBone = false;
}

void THudLightLaser::NewTorchlight(const char* section)
{
	ClearActiveLaserDotForOwner(this);
	ClearLaserZeroCacheForOwner(this);

	if (RenderLight)
	{
		RenderLight.destroy();
	}

	if (OmniLight)
	{
		OmniLight.destroy();
	}

	if (!pSettings->line_exist(section, "laser_installed"))
	{
		return;
	}

	Section = section;
	LightBone = READ_IF_EXISTS(pSettings, r_string, section, "laserdot_attach_bone", "wpn_body");

	LaserLightDist = READ_IF_EXISTS(pSettings, r_float, section, "laser_light_distance", 15.0f);

	LaserWorkDist = READ_IF_EXISTS(pSettings, r_float, section, "laser_work_distance",
		READ_IF_EXISTS(pSettings, r_float, section, "laser_wrok_distance", LaserLightDist * 0.5f));
	LaserMaxDist = READ_IF_EXISTS(pSettings, r_float, section, "laser_max_distance", std::max(LaserWorkDist, 80.0f));

	LightColor = READ_IF_EXISTS(pSettings, r_fcolor, section, "laser_light_color", LightColor.set(10.0f, 0.30f, 0.15f, 1.0f));

	RenderLight = ::Render->light_create();

	Fcolor initialSpotColor;
	Fvector initialDotSrgb;
	ResolveWeaponLaserColors(LightColor, initialSpotColor, initialDotSrgb);
	RenderLight->set_color(initialSpotColor);
	RenderLight->set_range(LaserLightDist);
	RenderLight->set_virtual_size(0.02f);
	RenderLight->set_type(IRender_Light::SPOT);
	RenderLight->set_occq_mode(false);

	// Unshadowed spot keeps get_LOD() == 1 at long range while still projecting s_lmap in R4.
	RenderLight->set_shadow(!!READ_IF_EXISTS(pSettings, r_bool, section, "laser_render_shadow", false));

	LightSpotAngle = READ_IF_EXISTS(pSettings, r_fvector2, section, "laser_spot_angle", LightSpotAngle.set(2, 5));
	LightSpotAngle.mul(M_PI / 180);

	RenderLight->set_cone(LightSpotAngle.x);
	RenderLight->set_texture(ResolveLaserSpotTexture(section));

	LightOffset.x = READ_IF_EXISTS(pSettings, r_float, section, "laserdot_attach_offset_x", 0.0f);
	LightOffset.y = READ_IF_EXISTS(pSettings, r_float, section, "laserdot_attach_offset_y", 0.0f);
	LightOffset.z = READ_IF_EXISTS(pSettings, r_float, section, "laserdot_attach_offset_z", 0.0f);

	LightWorldOffset.x = READ_IF_EXISTS(pSettings, r_float, section, "laserdot_world_attach_offset_x", 0.0f);
	LightWorldOffset.y = READ_IF_EXISTS(pSettings, r_float, section, "laserdot_world_attach_offset_y", 0.0f);
	LightWorldOffset.z = READ_IF_EXISTS(pSettings, r_float, section, "laserdot_world_attach_offset_z", 0.0f);

	LightWorldOffset = READ_IF_EXISTS(pSettings, r_fvector3, section, "laserdot_world_attach_offset", LightWorldOffset);
	LightOffset = READ_IF_EXISTS(pSettings, r_fvector3, section, "laserdot_attach_offset", LightOffset);

	if (pSettings->line_exist(section, "laser_ray_bones"))
	{
		ConeBones.clear();
		string128 bone_name = {};

		const char* lineStr = pSettings->r_string(section, "laser_ray_bones");
		for (auto i = 0, cnt = _GetItemCount(lineStr); i < cnt; ++i)
		{
			ConeBones.push_back(_GetItem(lineStr, i, bone_name));
		}
	}

	Fvector tmp_vector = { -1.0f, -1.0f, 0.0f };

	tmp_vector = READ_IF_EXISTS(pSettings, r_fvector3, section, "laser_breaking_params", tmp_vector);
	BreakingParams.start_condition = tmp_vector.x;
	BreakingParams.end_condition = tmp_vector.y;
	BreakingParams.start_probability = tmp_vector.z;
	BreakingParams.levels_problem = READ_IF_EXISTS(pSettings, r_float, section, "laser_problems_level", 0.0f);
}

bool forceLPDraw = false;
void THudLightLaser::UpdateTorchFromObject(CHudItem* item) const
{
	if (RenderLight == nullptr || item == nullptr || item->object().Visual() == nullptr)
	{
		ClearActiveLaserDotForOwner(this);
		return;
	}

	if (item->object().H_Parent())
	{
		if (item->GetState() == item->eHidden || item->cast_inventory_item()->CurrPlace() == eItemPlace::eItemPlaceRuck)
		{
			RenderLight->set_active(false);
			ClearActiveLaserDotForOwner(this);
			return;
		}
	}

	if (!IsRenderLight)
	{
		RenderLight->set_active(false);
		ClearActiveLaserDotForOwner(this);
		return;
	}

	Fvector rayPos = { 0, 0, 0 };
	Fvector rayDir = { 0, 0, 1 };
	const bool isHudMode = item->GetHUDmode() && item->HudItemData() != nullptr;
	const bool useWeaponAxis = isHudMode && (g_WeaponLaserAimMode >= 1);
	CWeapon* hudWpn = nullptr;
	CActor* hudActor = nullptr;
	Fvector hudFallbackEmitter = { 0, 0, 0 };
	Fvector hudZeroRestCam = { 0.0f, 0.0f, 1.0f };
	bool hasHudFallbackEmitter = false;
	bool hasHudZeroRest = false;
	u16 hudLightBoneId = BI_NONE;

	if (isHudMode)
	{
		CWeapon* wpn = item->cast_weapon();
		CActor* actor = item->object().H_Parent() ? item->object().H_Parent()->cast_actor() : nullptr;
		hudWpn = wpn;
		hudActor = actor;
		const attachable_hud_item* hudData = item->HudItemData();
		hudLightBoneId = (hudData->m_model != nullptr && LightBone != nullptr) ? hudData->m_model->LL_BoneID(LightBone) : BI_NONE;

		const u8 state = item->GetState();
		const bool weaponReady =
			state == CHudItem::eIdle ||
			(wpn != nullptr && (state == CWeapon::eFire || state == CWeapon::eFire2 || state == CWeapon::eEmptyClick));
		const bool actorSprinting = actor != nullptr &&
			(actor->GetMovementState(ACTOR_DEFS::EMovementStates::eReal) & ACTOR_DEFS::EMoveCommand::mcSprint) != 0;
		const bool actorSafemode = actor != nullptr && actor->IsSafemode();

		if (ShouldSuppressHudLaser(g_WeaponLaserAimMode, weaponReady, actorSprinting, actorSafemode))
		{
			RenderLight->set_active(false);
			ClearActiveLaserDotForOwner(this);
			return;
		}

		bool haveWeaponAxisRay = false;
		if (useWeaponAxis)
		{
			Fvector rawBoneDirCam;
			if (TryComputeHudBoneDirCam(hudData, hudLightBoneId, rawBoneDirCam))
			{
				if (g_LaserZeroCache.owner != this ||
					g_LaserZeroCache.hudSection != hudData->m_sect_name ||
					g_LaserZeroCache.lightBoneId != hudLightBoneId)
				{
					g_LaserZeroCache.Reset();
					g_LaserZeroCache.owner = this;
					g_LaserZeroCache.hudSection = hudData->m_sect_name;
					g_LaserZeroCache.lightBoneId = hudLightBoneId;
				}

				const EHudOffsetType hudOffsetIdx = item->GetCurrentHudOffsetIdx();
				const bool isAimIdx = (hudOffsetIdx == EHudOffsetType::eAim || hudOffsetIdx == EHudOffsetType::eAimAlt);
				if (isAimIdx && g_LaserZeroCache.aimOffsetIdx != hudOffsetIdx)
				{
					g_LaserZeroCache.calib.ResetAim();
					g_LaserZeroCache.aimOffsetIdx = hudOffsetIdx;
				}

				const Fvector& aimRot = hudData->m_measures.m_hands_positions.hands_offsets[1][g_LaserZeroCache.aimOffsetIdx];
				const Fvector& savedRot = hudData->m_measures.m_hands_positions.hands_offsets_saved[1];
				const Fvector& defaultHandsRot = hudData->m_measures.m_hands_positions.hands_offsets_tune[1][EHudOffsetType::eDefault];
				const float aimFactor = item->GetAimFactor();
				const float safeModeFactor = (wpn != nullptr) ? wpn->m_fSafeModeRotationFactor : 0.0f;
				const bool isSafemode = actorSafemode || hudOffsetIdx == EHudOffsetType::eSafemode || safeModeFactor > 0.01f;
				const bool isGrenadeMode = (hudOffsetIdx == EHudOffsetType::eAimGL) || (wpn != nullptr && wpn->IsGrenadeMode());
				const bool actorMoving = actor != nullptr && actor->AnyMove();
				const bool camTurning = actor != nullptr &&
					(std::abs(actor->fFPCamYawMagnitude) >= 0.02f || std::abs(actor->fFPCamPitchMagnitude) >= 0.02f);
				const bool noCollision = hudData->m_collision_inertia_rot.square_magnitude() < 1e-6f;
				const bool noInertionRot = hudData->m_measures.m_hands_attach_real[1].similar(defaultHandsRot, 0.08f);

				SLaserZeroSampleInput sampleIn;
				sampleIn.boneDirCam = { rawBoneDirCam.x, rawBoneDirCam.y, rawBoneDirCam.z };
				sampleIn.aimRot = { aimRot.x, aimRot.y, aimRot.z };
				sampleIn.savedRot = { savedRot.x, savedRot.y, savedRot.z };
				sampleIn.aimFactor = aimFactor;
				sampleIn.dt = (g_LaserZeroCache.lastFrame != Device.dwFrame) ? Device.fTimeDelta : 0.0f;
				g_LaserZeroCache.lastFrame = Device.dwFrame;
				sampleIn.isIdle = (state == CHudItem::eIdle);
				sampleIn.isIdleSettled = sampleIn.isIdle && (item->CurrStateTime() >= 200u);
				sampleIn.isStillNow = !actorMoving && !actorSprinting && !isSafemode && !isGrenadeMode &&
					!camTurning && noCollision && noInertionRot;
				sampleIn.isHipSettled = (aimFactor < 0.01f) && (hudOffsetIdx == EHudOffsetType::eDefault) &&
					(savedRot.square_magnitude() < 1e-6f);
				sampleIn.isAimSettled = (aimFactor > 0.99f) && isAimIdx && savedRot.similar(aimRot, 1e-4f);
				sampleIn.isSafemode = isSafemode;
				sampleIn.isGrenadeMode = isGrenadeMode;

				SLaserVec3 restCamOut;
				hasHudZeroRest = StepLaserZeroCalibration(g_LaserZeroCache.calib, sampleIn, restCamOut);
				if (hasHudZeroRest)
				{
					hudZeroRestCam.set(restCamOut.x, restCamOut.y, restCamOut.z);
				}
			}

			haveWeaponAxisRay = ComputeHudWeaponAxisRay(
				hudData,
				hudLightBoneId,
				LightOffset,
				hasHudZeroRest ? &hudZeroRestCam : nullptr,
				item->object().H_Root(),
				hudFallbackEmitter,
				rayPos,
				rayDir);
			hasHudFallbackEmitter = haveWeaponAxisRay;
		}

		if (!haveWeaponAxisRay)
		{
			if (actor != nullptr)
			{
				actor->g_fireParams(item, rayPos, rayDir);
			}
			else
			{
				rayPos = Device.vCameraPosition;
				rayDir = Device.vCameraDirection;
			}
			rayDir.normalize_safe();

			// Follow HUD weapon collision deflection (attachable_hud_item::update_hud_additional)
			// so approaching an obstacle tilts the laser beam upward along with the barrel.
			const Fvector& collRot = hudData->m_collision_inertia_rot;
			const Fvector& collPos = hudData->m_collision_inertia_pos;
			if (collRot.square_magnitude() > EPS_S || collPos.square_magnitude() > EPS_S)
			{
				Fvector camRight = Device.vCameraRight;
				Fvector camUp;
				camUp.crossproduct(rayDir, camRight);
				if (camUp.square_magnitude() > EPS_S)
				{
					camUp.normalize();
					camRight.crossproduct(camUp, rayDir);
					camRight.normalize();
				}
				else
				{
					Fvector::generate_orthonormal_basis_normalized(rayDir, camUp, camRight);
				}

				Fmatrix camBasis;
				camBasis.identity();
				camBasis.i.set(camRight);
				camBasis.j.set(camUp);
				camBasis.k.set(rayDir);

				if (collPos.square_magnitude() > EPS_S)
				{
					Fvector worldPosOffset;
					camBasis.transform_dir(worldPosOffset, collPos);
					const float offsetLen = worldPosOffset.magnitude();
					if (offsetLen > EPS_S)
					{
						Fvector offsetDir = worldPosOffset;
						offsetDir.div(offsetLen);
						collide::rq_result shiftRQ;
						shiftRQ.range = offsetLen;
						if (g_pGameLevel->ObjectSpace.RayPick(rayPos, offsetDir, shiftRQ.range, collide::rqtBoth, shiftRQ, item->object().H_Root()))
						{
							const float safeDist = std::max(0.0f, shiftRQ.range - 0.03f);
							rayPos.mad(offsetDir, safeDist);
						}
						else
						{
							rayPos.add(worldPosOffset);
						}
					}
				}

				if (collRot.square_magnitude() > EPS_S)
				{
					Fmatrix hudCollision;
					hudCollision.identity();
					hudCollision.rotateX(collRot.x);

					Fmatrix hudCollisionY;
					hudCollisionY.identity();
					hudCollisionY.rotateY(collRot.y);
					hudCollision.mulA_43(hudCollisionY);

					Fmatrix hudCollisionZ;
					hudCollisionZ.identity();
					hudCollisionZ.rotateZ(collRot.z);
					hudCollision.mulA_43(hudCollisionZ);

					camBasis.transform_dir(rayDir, hudCollision.k);
					rayDir.normalize_safe();
				}
			}
		}
	}
	else
	{
		ClearActiveLaserDotForOwner(this);

		const Fmatrix& xform = item->object().XFORM();
		IKinematics* kin = item->object().Visual()->dcast_PKinematics();
		if (kin == nullptr)
		{
			RenderLight->set_active(false);
			return;
		}

		const u16 lightBoneId = kin->LL_BoneID(LightBone);
		if (lightBoneId == BI_NONE)
		{
			RenderLight->set_active(false);
			return;
		}

		kin->LL_GetTransform(lightBoneId).transform_tiny(rayPos, LightWorldOffset);
		xform.transform_tiny(rayPos);
		xform.transform_dir(rayDir);
		rayDir.normalize_safe();
	}

	collide::rq_result RQ;
	RQ.range = LaserMaxDist;
	const bool hasHit = !!g_pGameLevel->ObjectSpace.RayPick(rayPos, rayDir, RQ.range, collide::rqtBoth, RQ, item->object().H_Root());

	const float sizeScale = std::clamp(g_WeaponLaserSize, 0.5f, 2.5f);
	Fcolor spotColor;
	Fvector dotSrgb;
	ResolveWeaponLaserColors(LightColor, spotColor, dotSrgb);

	Fvector hitPos;
	float hipVisibleDiam = 0.010f * sizeScale;
	if (hasHit)
	{
		const float hitDist = useWeaponAxis ? std::max(RQ.range, 0.001f) : std::max(RQ.range, 0.05f);
		hitPos.mad(rayPos, rayDir, hitDist);

		// Place the spotlight closely in front of the impact point with a 2.2x range ratio
		// (GetLightAttention ~0.445 vs ~0.186 at 1.55x) while terminating shortly behind thin targets.
		const float maxStandOff = useWeaponAxis
			? std::min(std::max(hitDist * 0.9f, 0.08f), 1.1f)
			: std::min(hitDist * 0.9f, 1.1f);
		const float standOff = std::clamp(0.30f + hitDist * 0.02f, std::min(0.08f, maxStandOff), maxStandOff);
		const float lightRange = std::max(0.45f, standOff * 2.2f);

		Fvector lightPos;
		lightPos.mad(hitPos, rayDir, -standOff);

		// internal_light_laser_tactical_r2.dds bright core occupies ~11.5/256 (0.045) of the frustum (cone + 5 deg).
		const float sizeDist = useWeaponAxis
			? std::max(Device.vCameraPosition.distance_to(hitPos), 0.05f)
			: hitDist;
		hipVisibleDiam = ComputeLaserHipVisibleDiameter(sizeDist, sizeScale);
		const float zoomScale = isHudMode ? ComputeCameraZoomScale() : 1.0f;
		const float visibleDiam = std::max(0.002f * sizeScale, hipVisibleDiam / zoomScale);
		const float desiredHalfTan = visibleDiam / (2.0f * 0.045f * std::max(standOff, 0.05f));
		const float desiredFov = 2.0f * std::atan(desiredHalfTan);
		const float currentCone = std::clamp(desiredFov - deg2rad(5.0f), deg2rad(0.5f), deg2rad(60.0f));

		Fvector up, right;
		Fvector::generate_orthonormal_basis_normalized(rayDir, up, right);

		RenderLight->set_color(spotColor);
		RenderLight->set_range(lightRange);
		RenderLight->set_position(lightPos);
		RenderLight->set_rotation(rayDir, right);
		RenderLight->set_cone(currentCone);
		RenderLight->set_ignore_object(item->object().H_Root());
		RenderLight->set_active(true);
	}
	else
	{
		RenderLight->set_active(false);
		if (!isHudMode || (!useWeaponAxis && g_WeaponLaserBeamMode <= 0))
		{
			ClearActiveLaserDotForOwner(this);
			return;
		}
		hitPos.mad(rayPos, rayDir, LaserMaxDist);
	}

	if (isHudMode)
	{
		const attachable_hud_item* hudData = item->HudItemData();
		const bool canRenderBeam = item->need_renderable();
		if (!hasHit && !useWeaponAxis && (!canRenderBeam || g_WeaponLaserBeamMode <= 0))
		{
			ClearActiveLaserDotForOwner(this);
			return;
		}

		SLaserBeamEnvInput envIn;
		if (g_pGamePersistent != nullptr &&
			g_pGamePersistent->pEnvironment != nullptr &&
			g_pGamePersistent->Environment().CurrentEnv != nullptr)
		{
			const CEnvDescriptorMixer& env = *g_pGamePersistent->Environment().CurrentEnv;
			auto rgbLum = [](const Fvector3& c) -> float
			{
				return 0.2126f * std::max(0.0f, c.x) + 0.7152f * std::max(0.0f, c.y) + 0.0722f * std::max(0.0f, c.z);
			};
			envIn.sunLum = rgbLum(env.sun_color);
			envIn.hemiLum = 0.2126f * std::max(0.0f, env.hemi_color.x)
				+ 0.7152f * std::max(0.0f, env.hemi_color.y)
				+ 0.0722f * std::max(0.0f, env.hemi_color.z);
			envIn.ambientLum = rgbLum(env.ambient);
			envIn.rainDensity = env.rain_density;
			envIn.fogDistance = env.fog_distance;
			envIn.fogDensity = env.fog_density;
		}
		envIn.visDistance = psVisDistance;
		envIn.isIndoorRender = (::Render != nullptr && ::Render->InIndoor());
		if (hudActor != nullptr && hudActor->renderable_ROS() != nullptr)
		{
			envIn.actorHemi = hudActor->renderable_ROS()->get_luminocity_hemi();
		}

		const SLaserBeamEnvResult envRes = EvaluateLaserBeamEnvironment(envIn);
		if (g_SmoothedBeamAutoVis < 0.0f)
		{
			g_SmoothedBeamAutoVis = envRes.autoVisibility;
			g_SmoothedBeamDayGain = envRes.dayGain;
			g_SmoothedBeamAtmospheric = envRes.atmospheric;
			g_SmoothedBeamFrame = Device.dwFrame;
		}
		else if (g_SmoothedBeamFrame != Device.dwFrame)
		{
			const float smoothAlpha = std::clamp(Device.fTimeDelta * 3.5f, 0.0f, 1.0f);
			g_SmoothedBeamAutoVis += (envRes.autoVisibility - g_SmoothedBeamAutoVis) * smoothAlpha;
			g_SmoothedBeamDayGain += (envRes.dayGain - g_SmoothedBeamDayGain) * smoothAlpha;
			g_SmoothedBeamAtmospheric += (envRes.atmospheric - g_SmoothedBeamAtmospheric) * smoothAlpha;
			g_SmoothedBeamFrame = Device.dwFrame;
		}

		Fvector fallbackEmitter = hudFallbackEmitter;
		if (!hasHudFallbackEmitter)
		{
			fallbackEmitter = LightOffset;
			if (hudData->m_model != nullptr && hudLightBoneId != BI_NONE)
			{
				hudData->m_model->LL_GetTransform(hudLightBoneId).transform_tiny(fallbackEmitter, LightOffset);
			}
			hudData->m_item_transform.transform_tiny(fallbackEmitter);
			Device.transform_hud2world(fallbackEmitter);
		}

		g_ActiveLaserDot.owner = this;
		g_ActiveLaserDot.hudItem = hudData;
		g_ActiveLaserDot.hudAttachIdx = hudData->m_attach_place_idx;
		g_ActiveLaserDot.lightBoneId = hudLightBoneId;
		g_ActiveLaserDot.frame = Device.dwFrame;
		g_ActiveLaserDot.hitPos = hitPos;
		g_ActiveLaserDot.rayDir = rayDir;
		g_ActiveLaserDot.lightOffset = LightOffset;
		g_ActiveLaserDot.fallbackEmitterWorld = fallbackEmitter;
		g_ActiveLaserDot.beamTargetPos = hitPos;
		g_ActiveLaserDot.dotSrgb = dotSrgb;
		g_ActiveLaserDot.visibleDiam = hipVisibleDiam;
		g_ActiveLaserDot.sizeScale = sizeScale;
		g_ActiveLaserDot.brightness = std::clamp(g_WeaponLaserBrightness, 0.1f, 5.0f);
		g_ActiveLaserDot.aimFactor = item->GetAimFactor();
		g_ActiveLaserDot.beamAutoVis = g_SmoothedBeamAutoVis;
		g_ActiveLaserDot.beamDayGain = g_SmoothedBeamDayGain;
		g_ActiveLaserDot.beamAtmospheric = g_SmoothedBeamAtmospheric;
		g_ActiveLaserDot.laserMaxDist = LaserMaxDist;
		g_ActiveLaserDot.textureName = ResolveLaserSpotTexture(Section);
		g_ActiveLaserDot.hasDot = hasHit;
		g_ActiveLaserDot.hasBeam = canRenderBeam;
		g_ActiveLaserDot.hasScopeTexture = (hudWpn != nullptr && hudWpn->ZoomTexture() != nullptr);
		g_ActiveLaserDot.useWeaponAxis = useWeaponAxis;
		g_ActiveLaserDot.hasZeroRest = hasHudZeroRest;
		g_ActiveLaserDot.zeroRestCam = hudZeroRestCam;
		g_ActiveLaserDot.active = true;
	}

#ifdef DEBUG_DRAW
	if (isHudMode && hasHit && forceLPDraw)
	{
		Fobb obb;
		Fmatrix trans = item->HudItemData()->m_item_transform;
		obb.m_rotate.i.set(trans.i);
		obb.m_rotate.j.set(trans.j);
		obb.m_rotate.k.set(trans.k);
		obb.m_halfsize.set(0.005f, 0.005f, 0.005f);
		obb.m_translate.set(hitPos);
		obb.xform_full(trans);
		Level().debug_renderer().draw_obb(trans, color_xrgb(255, 0, 0));
	}
#endif
}

void THudLightLaser::SwitchLaser(bool& saved_status, bool status, bool forced)
{
	if (!forced && status == saved_status)
	{
		return;
	}

	saved_status = status;

	if (!status)
	{
		ClearActiveLaserDotForOwner(this);
	}

	SwitchTorchlight(status);
}

void THudLightLaser::UpdateLaser(CHudItemObject* item, bool& saved_status)
{
	SwitchLaser(saved_status, saved_status, true);

	bool is_broken = false;
	const float current_condition = item->GetCondition();
	const int current_problems_cnt = Level().GetElectronicsProblemsManager()->CurrentElectronicsProblemsCnt();
	const int target_problems_cnt = Level().GetElectronicsProblemsManager()->TargetElectronicsProblemsCnt();

	const THudLightLaser::breaking_params& BreakingParams = this->BreakingParams;

	if (current_condition < BreakingParams.end_condition)
	{
		is_broken = true;
	}
	else if (current_condition < BreakingParams.start_condition || BreakingParams.levels_problem > 0.0f && current_problems_cnt >= BreakingParams.levels_problem)
	{
		float probability = 0.0f;

		if (target_problems_cnt >= BreakingParams.levels_problem)
		{
			probability = 1.0f;
		}
		else if (BreakingParams.start_condition == BreakingParams.end_condition)
		{
			probability = BreakingParams.start_condition;
		}
		else
		{
			probability = BreakingParams.start_probability + (BreakingParams.start_condition - current_condition) * (1.0f - BreakingParams.start_probability) / (BreakingParams.start_condition - BreakingParams.end_condition);
		}

		is_broken = !!(::Random.randF(0.0f, 1.0f) < probability);
	}

	if (is_broken)
	{
		SwitchTorchlight(false);
	}

	auto SetVisible = [&](IKinematics* kin, const shared_str& bone_name, bool status)
	{
		if (kin != nullptr)
		{
			u16 bone_id = kin->LL_BoneID(bone_name);
			if (bone_id != BI_NONE)
			{
				kin->LL_SetBoneVisible(bone_id, status, false);
			}
		}
	};

	attachable_hud_item* HID = item->HudItemData();
	IKinematics* hud_kin = HID != nullptr ? HID->m_model : nullptr;
	IKinematics* world_kin = item->Visual() != nullptr ? PKinematics(item->Visual()) : nullptr;

	for (const shared_str& bone : ConeBones)
	{
		SetVisible(hud_kin, bone, GetTorchActive());
		SetVisible(world_kin, bone, GetTorchActive());
	}

	UpdateTorchFromObject(item);
}