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
#include <algorithm>
#include <cmath>

void THudLightTorch::BeginComponent(IECSOwner* O)
{
	SetInstalled(true);
}

void THudLightTorch::EndComponent()
{
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

namespace
{
	int g_WeaponLaserColorMode = 0; // 0 = weapon config (LTX), 1 = red 635nm, 2 = green 520nm, 3 = blue 450nm, 4 = yellow
	float g_WeaponLaserBrightness = 1.0f;
	float g_WeaponLaserSize = 1.0f;

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

	Fcolor ResolveWeaponLaserColor(const Fcolor& configColor)
	{
		Fcolor base = configColor;
		switch (g_WeaponLaserColorMode)
		{
		case 1: // Red 635 nm (L3Harris LA-5B/PEQ)
			base.set(10.0f, 0.30f, 0.15f, 1.0f);
			break;
		case 2: // Green 520 nm
			base.set(0.30f, 10.0f, 0.35f, 1.0f);
			break;
		case 3: // Blue 450 nm
			base.set(0.25f, 1.20f, 12.0f, 1.0f);
			break;
		case 4: // Yellow / amber
			base.set(9.0f, 6.50f, 0.20f, 1.0f);
			break;
		default:
			break;
		}

		const float scale = std::clamp(g_WeaponLaserBrightness, 0.1f, 5.0f);
		Fcolor out;
		out.set(base.r * scale, base.g * scale, base.b * scale, base.a);
		return out;
	}
} // namespace

void RegisterWeaponLaserConsoleCommands()
{
	CMD4(CCC_WeaponLaserInteger, "wpn_laser_color", &g_WeaponLaserColorMode, 0, 4);
	CMD4(CCC_WeaponLaserFloat, "wpn_laser_brightness", &g_WeaponLaserBrightness, 0.1f, 5.0f);
	CMD4(CCC_WeaponLaserFloat, "wpn_laser_size", &g_WeaponLaserSize, 0.5f, 2.5f);
}

void THudLightLaser::BeginComponent(IECSOwner* O)
{
	THudLightTorch::BeginComponent(O);
	IsLightDirByBone = false;
}

void THudLightLaser::NewTorchlight(const char* section)
{
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

	RenderLight->set_color(ResolveWeaponLaserColor(LightColor));
	RenderLight->set_range(LaserLightDist);
	RenderLight->set_virtual_size(0.02f);
	RenderLight->set_type(IRender_Light::SPOT);

	// Unshadowed spot keeps get_LOD() == 1 at long range while still projecting s_lmap in R4.
	RenderLight->set_shadow(!!READ_IF_EXISTS(pSettings, r_bool, section, "laser_render_shadow", false));

	LightSpotAngle = READ_IF_EXISTS(pSettings, r_fvector2, section, "laser_spot_angle", LightSpotAngle.set(2, 5));
	LightSpotAngle.mul(M_PI / 180);

	RenderLight->set_cone(LightSpotAngle.x);
	RenderLight->set_texture(READ_IF_EXISTS(pSettings, r_string, section, "laser_spot_texture", "internal\\internal_light_laser_tactical_r2"));

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
		return;
	}

	if (item->object().H_Parent())
	{
		if (item->GetState() == item->eHidden || item->cast_inventory_item()->CurrPlace() == eItemPlace::eItemPlaceRuck)
		{
			RenderLight->set_active(false);
			return;
		}
	}

	if (!IsRenderLight)
	{
		RenderLight->set_active(false);
		return;
	}

	Fvector rayPos = { 0, 0, 0 };
	Fvector rayDir = { 0, 0, 1 };
	const bool isHudMode = item->GetHUDmode() && item->HudItemData() != nullptr;

	if (isHudMode)
	{
		CWeapon* wpn = item->cast_weapon();
		CActor* actor = item->object().H_Parent() ? item->object().H_Parent()->cast_actor() : nullptr;
		const u8 state = item->GetState();
		const bool weaponReady =
			state == CHudItem::eIdle ||
			(wpn != nullptr && (state == CWeapon::eFire || state == CWeapon::eFire2 || state == CWeapon::eEmptyClick));
		const bool actorSprinting = actor != nullptr &&
			(actor->GetMovementState(ACTOR_DEFS::EMovementStates::eReal) & ACTOR_DEFS::EMoveCommand::mcSprint) != 0;
		const bool actorSafemode = actor != nullptr && actor->IsSafemode();

		if (!weaponReady || actorSprinting || actorSafemode)
		{
			RenderLight->set_active(false);
			return;
		}

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
	}
	else
	{
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
	if (!g_pGameLevel->ObjectSpace.RayPick(rayPos, rayDir, RQ.range, collide::rqtBoth, RQ, item->object().H_Root()))
	{
		RenderLight->set_active(false);
		return;
	}

	const float hitDist = std::max(RQ.range, 0.05f);
	Fvector hitPos;
	hitPos.mad(rayPos, rayDir, hitDist);

	// Place the spotlight closely in front of the impact point so attenuation stays uniform
	// across distance and the beam terminates shortly behind thin surfaces / NPCs.
	const float maxStandOff = std::min(hitDist * 0.9f, 2.2f);
	const float standOff = std::clamp(0.6f + hitDist * 0.03f, std::min(0.08f, maxStandOff), maxStandOff);
	const float lightRange = std::max(0.45f, standOff * 1.55f);

	Fvector lightPos;
	lightPos.mad(hitPos, rayDir, -standOff);

	// internal_light_laser_tactical_r2.dds dot occupies ~14/256 (0.0547) of the projected frustum (cone + 5 deg).
	const float sizeScale = std::clamp(g_WeaponLaserSize, 0.5f, 2.5f);
	const float targetDotDiam = std::clamp(0.008f + hitDist * 0.0022f, 0.008f, 0.20f) * sizeScale;
	const float desiredHalfTan = targetDotDiam / (2.0f * 0.0547f * std::max(standOff, 0.05f));
	const float desiredFov = 2.0f * std::atan(desiredHalfTan);
	const float currentCone = std::clamp(desiredFov - deg2rad(5.0f), deg2rad(0.5f), deg2rad(60.0f));

	Fvector up, right;
	Fvector::generate_orthonormal_basis_normalized(rayDir, up, right);

	RenderLight->set_color(ResolveWeaponLaserColor(LightColor));
	RenderLight->set_range(lightRange);
	RenderLight->set_position(lightPos);
	RenderLight->set_rotation(rayDir, right);
	RenderLight->set_cone(currentCone);
	RenderLight->set_ignore_object(item->object().H_Root());
	RenderLight->set_active(true);

#ifdef DEBUG_DRAW
	if (isHudMode && forceLPDraw)
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