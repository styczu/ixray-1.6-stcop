////////////////////////////////////////////////////////////////////////////
//	Module 		: UIActorStateInfo.cpp
//	Created 	: 15.02.2008
//	Author		: Evgeniy Sokolov
//	Description : UI actor state window class implementation
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "UIActorStateInfo.h"
#include "../../xrUI/Widgets/UIProgressBar.h"
#include "../../xrUI/Widgets/UIProgressShape.h"
#include "../../xrUI/Widgets/UIScrollView.h"
#include "../../xrUI/Widgets/UIFrameWindow.h"
#include "../../xrUI/Widgets/UIStatic.h"
#include "../../xrUI/UIXmlInit.h"
#include "object_broker.h"

#include "UIHelperGame.h"
#include "../../xrUI/Widgets/UIArrow.h"
#include "UIHudStatesWnd.h"

#include "../Level.h"
#include "../location_manager.h"
#include "../player_hud.h"
#include "UIMainIngameWnd.h"
#include "UIGameCustom.h"

#include "../Actor.h"
#include "../ActorCondition.h"
#include "../EntityCondition.h"
#include "../CustomOutfit.h"
#include "../ActorHelmet.h"
#include "../Inventory.h"
#include "../Artefact.h"
#include "../BoneProtections.h"
#include "../../xrEngine/bone.h"
#include "../../Include/xrRender/Kinematics.h"
#include "../../xrEngine/string_table.h"

namespace
{
	// Grupy kosci pokazywane w tooltipie klasy pancerza. Dlonie i stopy sa
	// swiadomie pominiete - maja wszedzie wartosci szczatkowe. Twarz jest
	// osobno, bo bywa slabsza od czaszki i w jednej grupie z glowa znikalaby
	// pod maksimum.
	//
	// "section" dzieli tabele na dwie czesci pokazywane w tooltipie osobno:
	// 0 = to, co kryje glowe (helm ALBO zintegrowany kaptur kombinezonu),
	// 1 = reszta sylwetki. Z maksimum kazdej sekcji bierze sie para liczb
	// przy pasku, wiec liczba po lewej nie zalezy od tego, czy w slocie helmu
	// cos siedzi - exoszkielet ma oslone glowy we wlasnym bones_koeff_protection.
	struct SArmorGroup
	{
		int		section;
		LPCSTR	label_key;
		LPCSTR	bones[8];
	};

	static const SArmorGroup kArmorGroups[] =
	{
		{ 0, "ui_armor_tt_head",  { "bip01_head", nullptr } },
		{ 0, "ui_armor_tt_face",  { "eyelid_1", "eye_left", "eye_right", "jaw_1", nullptr } },
		{ 0, "ui_armor_tt_neck",  { "bip01_neck", nullptr } },
		{ 1, "ui_armor_tt_torso", { "bip01_pelvis", "bip01_spine", "bip01_spine1", "bip01_spine2",
								   "bip01_l_clavicle", "bip01_r_clavicle", nullptr } },
		{ 1, "ui_armor_tt_arms",  { "bip01_l_upperarm", "bip01_r_upperarm",
								   "bip01_l_forearm", "bip01_r_forearm", nullptr } },
		{ 1, "ui_armor_tt_legs",  { "bip01_l_thigh", "bip01_r_thigh",
								   "bip01_l_calf", "bip01_r_calf", nullptr } },
	};

	static const u32 kArmorGroupCount = sizeof(kArmorGroups) / sizeof(kArmorGroups[0]);

	// Nazwa klasy pancerza dla progu przebicia. Tabela siedzi w danych:
	//   [ui_armor_classes]
	//   thresholds = 0.00, 0.21, 0.33, ...
	//   names      = 0, 0a, 1, ...
	// Progi musza byc rosnaco. Brak sekcji = tooltip pokazuje same liczby.
	LPCSTR ArmorClassName(float value)
	{
		static bool						s_loaded = false;
		static xr_vector<float>			s_thresholds;
		static xr_vector<shared_str>	s_names;

		if (!s_loaded)
		{
			s_loaded = true;
			if (pSettings->section_exist("ui_armor_classes") &&
				pSettings->line_exist("ui_armor_classes", "thresholds") &&
				pSettings->line_exist("ui_armor_classes", "names"))
			{
				LPCSTR t = pSettings->r_string("ui_armor_classes", "thresholds");
				LPCSTR n = pSettings->r_string("ui_armor_classes", "names");
				string64 buf;
				const int t_count = _GetItemCount(t);
				for (int i = 0; i < t_count; ++i)
					s_thresholds.push_back((float)atof(_GetItem(t, i, buf)));

				const int n_count = _GetItemCount(n);
				for (int i = 0; i < n_count; ++i)
					s_names.push_back(_GetItem(n, i, buf));
			}
		}

		LPCSTR result = nullptr;
		const u32 count = _min((u32)s_thresholds.size(), (u32)s_names.size());
		for (u32 i = 0; i < count; ++i)
		{
			if (value + EPS_L >= s_thresholds[i])
				result = s_names[i].c_str();
		}
		return result;
	}

	// Najwyzszy efektywny prog przebicia w grupie. -1 = zadna kosc grupy
	// nie jest kryta przez zadna warstwe.
	float GroupArmor(IKinematics* ikv, CCustomOutfit* outfit, CHelmet* helmet, const SArmorGroup& group)
	{
		float best = -1.0f;
		for (u32 i = 0; group.bones[i] != nullptr; ++i)
		{
			const u16 bone_id = ikv->LL_BoneID(group.bones[i]);
			if (BI_NONE == bone_id)
				continue;

			if (outfit)
			{
				const float a = outfit->GetBoneArmor((s16)bone_id);
				if (a >= 0.0f)
					best = _max(best, a * outfit->GetCondition());
			}
			if (helmet)
			{
				const float a = helmet->GetBoneArmor((s16)bone_id);
				if (a >= 0.0f)
					best = _max(best, a * helmet->GetCondition());
			}
		}
		return best;
	}

	// Maksimum po grupach jednej sekcji. -1 = zadna kosc sekcji nie jest kryta
	// przez zadna warstwe.
	float SectionArmor(IKinematics* ikv, CCustomOutfit* outfit, CHelmet* helmet, int section)
	{
		float best = -1.0f;
		for (u32 g = 0; g < kArmorGroupCount; ++g)
		{
			if (kArmorGroups[g].section != section)
				continue;

			const float a = GroupArmor(ikv, outfit, helmet, kArmorGroups[g]);
			if (a > best)
				best = a;
		}
		return best;
	}

	// Kolory tooltipa. Podawane jako argumenty %s, nigdy w napisie formatu -
	// "%c" w formacie zjadloby printf. Format CUILines to %c[a,r,g,b].
	static LPCSTR kColTitle = "%c[255,224,230,234]";
	static LPCSTR kColSep   = "%c[255,96,104,108]";
	static LPCSTR kColLabel = "%c[255,176,182,186]";
	static LPCSTR kColValue = "%c[255,232,178,84]";
	static LPCSTR kColUnit  = "%c[255,132,140,144]";
	static LPCSTR kColClass = "%c[255,200,208,212]";

	// CUILines lamie linie na dwuznaku BACKSLASH+N, nie na znaku nowej linii
	// (UILine.cpp: ProcessNewLines szuka "\\n", tak samo galaz multibyte
	// w UILines.cpp). Prawdziwy '\n' przechodzi bez sladu - stad ten dwuznak.
	static LPCSTR kBreak = "\\n";

	// Liczba z separatorem dziesietnym z tablicy stringow. Panel uzywa
	// przecinka, wiec tooltip nie moze zostac przy kropce.
	void FormatRate(string32& out, float value, int decimals)
	{
		string32 fmt = { 0 };
		xr_sprintf(fmt, sizeof(fmt), "%%.%df", decimals);
		xr_sprintf(out, sizeof(out), fmt, value);

		const shared_str sep = g_pStringTable->translate("ui_uip_decimal_sep");
		if (sep.size() && sep.c_str()[0] != '.')
		{
			char* dot = strchr(out, '.');
			if (dot != nullptr)
				*dot = sep.c_str()[0];
		}
	}
}

ui_actor_state_wnd::~ui_actor_state_wnd()
{
	delete_data( m_hint_wnd );
}

void ui_actor_state_wnd::init_from_xml( CUIXml& xml, LPCSTR path )
{
	XML_NODE* stored_root = xml.GetLocalRoot();
	CUIXmlInit::InitWindow( xml, path, 0, this );

	XML_NODE* new_root = xml.NavigateToNode( path, 0 );
	xml.SetLocalRoot( new_root );

	m_hint_wnd = UIHelper::CreateHint( xml, "hint_wnd" );

	for ( int i = 0; i < stt_count; ++i )
	{
		m_state[i] = new ui_actor_state_item();
		m_state[i]->SetAutoDelete( true );
		AttachChild( m_state[i] );
		m_state[i]->set_hint_wnd( m_hint_wnd );
	}
	if (xml.NavigateToNode("stamina_state"))
		m_state[stt_stamina]->init_from_xml( xml, "stamina_state" );
	m_state[stt_health]->init_from_xml( xml, "health_state");
	if (xml.NavigateToNode("bleeding_state"))
		m_state[stt_bleeding]->init_from_xml( xml, "bleeding_state");
	if (xml.NavigateToNode("radiation_state"))
		m_state[stt_radiation]->init_from_xml( xml, "radiation_state");
	if (xml.NavigateToNode("armor_state"))
		m_state[stt_armor]->init_from_xml( xml, "armor_state");

	if (xml.NavigateToNode("main_sensor"))
		m_state[stt_main]->init_from_xml( xml, "main_sensor");
	m_state[stt_fire]->init_from_xml( xml, "fire_sensor");
	m_state[stt_radia]->init_from_xml( xml, "radia_sensor");
	m_state[stt_acid ]->init_from_xml( xml, "acid_sensor");
	m_state[stt_psi]->init_from_xml( xml, "psi_sensor");
	if (xml.NavigateToNode("wound_sensor"))
		m_state[stt_wound]->init_from_xml( xml, "wound_sensor");
	if (xml.NavigateToNode("fire_wound_sensor"))
		m_state[stt_fire_wound]->init_from_xml( xml, "fire_wound_sensor");
	if (xml.NavigateToNode("shock_sensor"))
		m_state[stt_shock]->init_from_xml( xml, "shock_sensor");
	if (xml.NavigateToNode("power_sensor"))
		m_state[stt_power]->init_from_xml( xml, "power_sensor");
	
	if (xml.NavigateToNode("starvation_state"))
		m_state[stt_satiety]->init_from_xml(xml, "starvation_state");
	if (xml.NavigateToNode("thirst_state"))
		m_state[stt_thirst]->init_from_xml(xml, "thirst_state");
	if (xml.NavigateToNode("sleeping_state"))
		m_state[stt_sleep]->init_from_xml(xml, "sleeping_state");

	xml.SetLocalRoot( stored_root );
}

void ui_actor_state_wnd::UpdateActorInfo(CInventoryOwner* owner)
{
	CActor* actor = owner->cast_actor();
	if (actor == nullptr)
	{
		return;
	}

	float value = 0.0f;

	if (!m_state[stt_health]->m_progress->IsExpressionSystem)
	{
		value = actor->conditions().GetHealth();
		value = floor(value * 55) / 55; // number of sticks in progress bar
		// show bleeding icon
		m_state[stt_health]->set_progress(value);
	}

	if (m_state[stt_thirst]->m_progress != nullptr && !m_state[stt_thirst]->m_progress->IsExpressionSystem)
	{
		value = actor->conditions().GetThirst();
		m_state[stt_thirst]->set_progress(value);
	}
	
	if (m_state[stt_satiety]->m_progress != nullptr && !m_state[stt_satiety]->m_progress->IsExpressionSystem)
	{
		value = actor->conditions().GetSatiety();
		m_state[stt_satiety]->set_progress(value);
	}
	
	if (m_state[stt_sleep]->m_progress != nullptr && !m_state[stt_sleep]->m_progress->IsExpressionSystem)
	{
		value = actor->conditions().GetSleepiness();
		m_state[stt_sleep]->set_progress(value);
	}

	value = actor->GetRestoreSpeed(ALife::ePowerRestoreSpeed);
	m_state[stt_stamina]->set_text(value); // 0..0.99

	// show bleeding icon
	value = actor->conditions().BleedingSpeed();
	m_state[stt_health]->show_static((value > 0.01f)); // Bleeding icon in Clear Sky
	
	m_state[stt_bleeding]->show_static(false, 1);
	m_state[stt_bleeding]->show_static(false, 2);
	m_state[stt_bleeding]->show_static(false, 3);
	if(!fis_zero(value, EPS))
	{
		if(value<0.35f)
			m_state[stt_bleeding]->show_static(true, 1);
		else if(value<0.7f)
			m_state[stt_bleeding]->show_static(true, 2);
		else 
			m_state[stt_bleeding]->show_static(true, 3);
	}
// show radiation icon
	value = actor->conditions().GetRadiation();
	m_state[stt_radiation]->show_static(false, 1);
	m_state[stt_radiation]->show_static(false, 2);
	m_state[stt_radiation]->show_static(false, 3);
	if(!fis_zero(value, EPS))
	{
		if(value<0.35f)
			m_state[stt_radiation]->show_static(true, 1);
		else if(value<0.7f)
			m_state[stt_radiation]->show_static(true, 2);
		else 
			m_state[stt_radiation]->show_static(true, 3);
	}
	m_state[stt_main]->set_progress_shape(value);

	CCustomOutfit* outfit = actor->GetOutfit();
	CHelmet* helmet = actor->GetHelmet();

	// Перепроверить все сеты, сбросы, проверки, можно сократить
	m_state[stt_fire_wound]->set_progress(0.0f);
	m_state[stt_fire]->set_progress(0.0f);
	m_state[stt_radia]->set_progress(0.0f);
	m_state[stt_acid]->set_progress(0.0f);
	m_state[stt_psi]->set_progress(0.0f);
	m_state[stt_wound]->set_progress(0.0f);
	m_state[stt_shock]->set_progress(0.0f);
	m_state[stt_power]->set_progress(0.0f);

	float fwou_value = 0.0f;
	float burn_value = 0.0f;
	float radi_value = 0.0f;
	float cmbn_value = 0.0f;
	float tele_value = 0.0f;
	float woun_value = 0.0f;
	float shoc_value = 0.0f;

	const auto& cur_booster_influences = actor->conditions().GetCurBoosterInfluences();
	CEntityCondition::BOOSTER_MAP::const_iterator it;
	it = cur_booster_influences.find(eBoostRadiationProtection);
	if (it != cur_booster_influences.end())
		radi_value += it->second.fBoostValue;

	it = cur_booster_influences.find(eBoostChemicalBurnProtection);
	if (it != cur_booster_influences.end())
		cmbn_value += it->second.fBoostValue;

	it = cur_booster_influences.find(eBoostTelepaticProtection);
	if (it != cur_booster_influences.end())
		tele_value += it->second.fBoostValue;

	if(outfit)
	{
		burn_value += outfit->GetDefHitTypeProtection(ALife::eHitTypeBurn);
		radi_value += outfit->GetDefHitTypeProtection(ALife::eHitTypeRadiation);
		cmbn_value += outfit->GetDefHitTypeProtection(ALife::eHitTypeChemicalBurn);
		tele_value += outfit->GetDefHitTypeProtection(ALife::eHitTypeTelepatic);
		woun_value += outfit->GetDefHitTypeProtection(ALife::eHitTypeWound);
		shoc_value += outfit->GetDefHitTypeProtection(ALife::eHitTypeShock);

		IKinematics* ikv = PKinematics(actor->Visual());
		VERIFY(ikv);
		u16 spine_bone = ikv->LL_BoneID("bip01_spine");

		value = outfit->GetBoneArmor(spine_bone);

		fwou_value += value * outfit->GetCondition();
		if(!outfit->bIsHelmetAvaliable)
		{
			u16 spine_bone_ = ikv->LL_BoneID("bip01_head");
			fwou_value += outfit->GetBoneArmor(spine_bone_)*outfit->GetCondition();
		}
	}

	if(helmet)
	{
		burn_value += helmet->GetDefHitTypeProtection(ALife::eHitTypeBurn);
		radi_value += helmet->GetDefHitTypeProtection(ALife::eHitTypeRadiation);
		cmbn_value += helmet->GetDefHitTypeProtection(ALife::eHitTypeChemicalBurn);
		tele_value += helmet->GetDefHitTypeProtection(ALife::eHitTypeTelepatic);
		woun_value += helmet->GetDefHitTypeProtection(ALife::eHitTypeWound);
		shoc_value += helmet->GetDefHitTypeProtection(ALife::eHitTypeShock);

		IKinematics* ikv = PKinematics(actor->Visual());
		VERIFY(ikv);
		u16 spine_bone = ikv->LL_BoneID("bip01_head");
		fwou_value += helmet->GetBoneArmor(spine_bone)*helmet->GetCondition();
	}
	const auto getProtection = [&](float& valueRef, ALife::EHitType hitType) -> float
		{
			valueRef += actor->GetProtection_ArtefactsOnBelt(hitType);
			return actor->conditions().GetZoneMaxPower(hitType);
		};

	// fire burn protection progress bar
	{
		const float max_power = getProtection(burn_value, ALife::eHitTypeBurn);
		update_round_states(stt_fire, burn_value, max_power);
	}
	// radiation protection progress bar
	{
		const float max_power = getProtection(radi_value, ALife::eHitTypeRadiation);
		update_round_states(stt_radia, radi_value, max_power);
	}
	// chemical burn protection progress bar
	{
		const float max_power = getProtection(cmbn_value, ALife::eHitTypeChemicalBurn);
		update_round_states(stt_acid, cmbn_value, max_power);
	}
	// telepathic protection progress bar
	{
		const float max_power = getProtection(tele_value, ALife::eHitTypeTelepatic);
		update_round_states(stt_psi, tele_value, max_power);
	}
	// wound protection progress bar
	{
		const float max_power = getProtection(woun_value, ALife::eHitTypeWound);
		update_round_states(stt_wound, woun_value, max_power);
	}
	// shock protection progress bar
	{
		const float max_power = getProtection(shoc_value, ALife::eHitTypeShock);
		update_round_states(stt_shock, shoc_value, max_power);
	}
	//fire wound protection progress bar
	{
		const float max_power = getProtection(fwou_value, ALife::eHitTypeFireWound);
		update_round_states(stt_fire_wound, fwou_value, max_power);
	}
	//power restore speed progress bar
	{
		if (m_state[stt_power]->m_progress && !m_state[stt_power]->m_progress->IsExpressionSystem)
		{
			value = actor->GetRestoreSpeed(ALife::ePowerRestoreSpeed) / actor->conditions().GetMaxPowerRestoreSpeed();
			value = floor(value * 31) / 31; // number of sticks in progress bar

			m_state[stt_power]->set_progress(value);
		}
	}
// -----------------------------------------------------------------------------------

	UpdateArmorInfo( actor, outfit, helmet );
	UpdateRateHints( actor );

	UpdateHitZone();
}

void ui_actor_state_wnd::UpdateArmorInfo(CActor* actor, CCustomOutfit* outfit, CHelmet* helmet)
{
	ui_actor_state_item* item = m_state[stt_armor];
	if (item == nullptr)
	{
		return;
	}

	// shared_str trzymany w zmiennej, a nie .c_str() z tymczasowego obiektu.
	const shared_str s_dash  = g_pStringTable->translate("ui_armor_tt_none");
	const shared_str s_title = g_pStringTable->translate("ui_armor_tt_title");
	const shared_str s_class = g_pStringTable->translate("ui_armor_tt_class");
	const shared_str s_sep   = g_pStringTable->translate("ui_armor_tt_sep");
	LPCSTR dash = s_dash.c_str();

	IKinematics* ikv = PKinematics(actor->Visual());
	if (ikv == nullptr)
	{
		item->set_text_str(dash);
		return;
	}

	// Liczba obok paska: "glowa/korpus" w setnych progu przebicia.
	// Obie polowy licza sie z OBU warstw naraz, wiec pusty slot helmu nie daje
	// kreski, jesli glowe kryje sam kombinezon (exoszkielet, kombinezony
	// z helmet_avaliable = false). Kreska oznacza teraz naprawde "nic nie kryje".
	const float head_armor = SectionArmor(ikv, outfit, helmet, 0);
	const float body_armor = SectionArmor(ikv, outfit, helmet, 1);

	string32 head_txt, body_txt;
	if (head_armor < 0.0f)
		xr_strcpy(head_txt, sizeof(head_txt), dash);
	else
		xr_sprintf(head_txt, sizeof(head_txt), "%d", iFloor(head_armor * 100.0f + 0.5f));

	if (body_armor < 0.0f)
		xr_strcpy(body_txt, sizeof(body_txt), dash);
	else
		xr_sprintf(body_txt, sizeof(body_txt), "%d", iFloor(body_armor * 100.0f + 0.5f));

	string64 value_txt;
	xr_sprintf(value_txt, sizeof(value_txt), "%s/%s", head_txt, body_txt);
	item->set_text_str(value_txt);

	// Tooltip: naglowek, pusta linia, potem dwie sekcje rozdzielone kreskowana
	// linia - najpierw to, co kryje glowe, potem reszta sylwetki.
	xr_string hint = kColTitle;
	hint += s_title.c_str();
	hint += kBreak;
	hint += kBreak;

	int last_section = -1;
	for (u32 g = 0; g < kArmorGroupCount; ++g)
	{
		if (kArmorGroups[g].section != last_section)
		{
			last_section = kArmorGroups[g].section;
			hint += kColSep;
			hint += s_sep.c_str();
			hint += kBreak;
		}

		const float a = GroupArmor(ikv, outfit, helmet, kArmorGroups[g]);
		const shared_str s_label = g_pStringTable->translate(kArmorGroups[g].label_key);

		// Kolumny trzymaja sie tylko przy foncie o stalej szerokosci - hint_wnd
		// panelu uzywa [ui_font_panel_tt] (consola). Strony kodowe 1250/1251/1252
		// sa jednobajtowe, wiec dopelnienie printf liczy znaki, nie bajty.
		string256 line;
		if (a < 0.0f)
		{
			xr_sprintf(line, sizeof(line), "%s%-8s %s%3s  %s%s %s%s",
				kColLabel, s_label.c_str(), kColValue, dash,
				kColUnit, s_class.c_str(), kColClass, dash);
		}
		else
		{
			LPCSTR cls = ArmorClassName(a);
			xr_sprintf(line, sizeof(line), "%s%-8s %s%3d  %s%s %s%s",
				kColLabel, s_label.c_str(), kColValue, iFloor(a * 100.0f + 0.5f),
				kColUnit, s_class.c_str(), kColClass, cls ? cls : dash);
		}

		hint += line;
		hint += kBreak;
	}

	item->set_hint_text(hint.c_str());
}

// Wartosci sekcji pancerza dla systemu wyrazen. Tabela kosci stoi wyzej
// w tym pliku, wiec delegaty nie moga jej dublowac.
namespace ActorArmor
{
	float SectionValue(CActor* actor, int section)
	{
		if (actor == nullptr)
			return -1.0f;

		IKinematics* ikv = PKinematics(actor->Visual());
		if (ikv == nullptr)
			return -1.0f;

		return SectionArmor(ikv, actor->GetOutfit(), actor->GetHelmet(), section);
	}
}

// Podpowiedzi wierszy, ktore musza pokazac wyliczona liczbe. System wyrazen
// nie sklada napisow (UI_ADD dziala tylko na int i float), wiec tekst
// powstaje tutaj - tak samo jak tooltip pancerza.
void ui_actor_state_wnd::UpdateRateHints(CActor* actor)
{
	auto& cv = actor->conditions().change_v();

	// Dopisuje linie "podpis liczba jednostka". unit_key = nullptr -> bez jednostki.
	auto add_line = [](xr_string& hint, LPCSTR label_key, float value, int decimals, LPCSTR unit_key)
	{
		string32 num;
		FormatRate(num, value, decimals);

		hint += kBreak;
		hint += g_pStringTable->translate(label_key).c_str();
		hint += " ";
		hint += num;
		if (unit_key != nullptr)
		{
			hint += " ";
			hint += g_pStringTable->translate(unit_key).c_str();
		}
	};

	// Skazenie: ubytek zdrowia to radiation_health_v * poziom skazenia,
	// w ulamku zdrowia na sekunde (EntityCondition.cpp, UpdateRadiation).
	if (m_state[stt_radiation] != nullptr)
	{
		const float drain = cv.m_fV_RadiationHealth * actor->conditions().GetRadiation();

		xr_string hint = g_pStringTable->translate("ui_uip_tt_rad").c_str();
		add_line(hint, "ui_uip_tt_rad_drain", drain * 100.0f, 3, "ui_uip_unit_hp_s");
		m_state[stt_radiation]->set_hint_text(hint.c_str());
	}

	// Krwawienie: ubytek to BleedingSpeed() * bleeding_v, a rany zablizniaja sie
	// tempem wound_incarnation_v + bleeding_restore_speed z kombinezonu
	// i artefaktow (UpdateHealth -> ChangeBleeding). Stad obie liczby w jednej
	// podpowiedzi - to dwie strony tego samego licznika.
	if (m_state[stt_bleeding] != nullptr)
	{
		const float drain = actor->conditions().BleedingSpeed() * cv.m_fV_Bleeding;
		const float heal  = cv.m_fV_WoundIncarnation + actor->conditions().GetBoostBleedingRestore();

		xr_string hint = g_pStringTable->translate("ui_uip_tt_bleed").c_str();
		add_line(hint, "ui_uip_tt_bleed_drain", drain * 100.0f, 3, "ui_uip_unit_hp_s");
		add_line(hint, "ui_uip_tt_bleed_heal",  heal * 100.0f,  3, "ui_uip_unit_wound_s");
		m_state[stt_bleeding]->set_hint_text(hint.c_str());
	}

	// Regeneracja. GetRestoreSpeed zbiera komplet - baze z actor.ltx, sytosc,
	// pragnienie, artefakty z pasa i kombinezon - i zwraca ULAMEK PASKA NA SEKUNDE,
	// wiec razy 100 daje procent na sekunde.
	//
	// Liczba przy pasku jest tym samym tempem razy 1000 (czyli promile na sekunde),
	// bo w polu 48 px nie miesci sie "0,020 %/s". To NIE jest ta sama skala,
	// co "+N" w opisie ulepszenia kombinezonu: tam property_functor_a wypisuje
	// surowe "value" z sekcji ulepszenia, recznie wpisana liczbe bez zwiazku
	// z parametrem (np. health_restore_speed = 0.0006 opisany jako "+4").
	if (m_state[stt_thirst] != nullptr)
	{
		const float rate = actor->GetRestoreSpeed(ALife::eHealthRestoreSpeed);

		xr_string hint = g_pStringTable->translate("ui_uip_tt_reg_health").c_str();
		add_line(hint, "ui_uip_reg_rate", rate * 100.0f, 3, "ui_uip_unit_hp_s");
		m_state[stt_thirst]->set_hint_text(hint.c_str());
	}

	if (m_state[stt_power] != nullptr)
	{
		const float rate = actor->GetRestoreSpeed(ALife::ePowerRestoreSpeed);

		xr_string hint = g_pStringTable->translate("ui_uip_tt_reg_power").c_str();
		add_line(hint, "ui_uip_reg_rate", rate * 100.0f, 3, "ui_uip_unit_st_s");
		m_state[stt_power]->set_hint_text(hint.c_str());
	}
}

void ui_actor_state_wnd::update_round_states(EStateType stt_type, float initial, float max_power)
{
	auto state = m_state[stt_type];

	const float progress = floor(initial / max_power * 31) / 31; // number of sticks in progress bar
	const float arrow = initial / max_power; //  = 0..1
	
	if (!state->set_progress(progress) && stt_type != stt_main)
	{
		state->set_arrow(arrow); // 0..1
		state->set_text(arrow); // 0..1
	}
}

void ui_actor_state_wnd::UpdateHitZone()
{
	CUIHudStatesWnd* wnd = CurrentGameUI()->UIMainIngameWnd->get_hud_states(); //некрасиво слишком
	VERIFY( wnd );
	if ( !wnd )
	{
		return;
	}
	wnd->UpdateZones();

	if (m_state[stt_main])
	{
		CActor* actor = Level().CurrentViewEntity() ? Level().CurrentViewEntity()->cast_actor() : NULL;
		m_state[stt_main]->set_arrow(actor->conditions().m_fRadiationZonePower);
	}

	/*
	m_state[stt_fire]->set_arrow(wnd->get_zone_cur_power(ALife::eHitTypeBurn));
	m_state[stt_radia]->set_arrow(nd->get_zone_cur_power(ALife::eHitTypeRadiation));
	m_state[stt_acid]->set_arrow(wnd->get_zone_cur_power(ALife::eHitTypeChemicalBurn));
	m_state[stt_psi]->set_arrow(wnd->get_zone_cur_power(ALife::eHitTypeTelepatic));
	*/
}

void ui_actor_state_wnd::Draw()
{
	inherited::Draw();
	m_hint_wnd->Draw();
}

void ui_actor_state_wnd::Show( bool status )
{
	inherited::Show( status );
	ShowChildren( status );
}

/// =============================================================================================
ui_actor_state_item::ui_actor_state_item()
{
	m_static		= nullptr;
	m_static2		= nullptr;
	m_static3		= nullptr;
	m_progress		= nullptr;
	m_sensor		= nullptr;
	m_arrow			= nullptr;
	m_arrow_shadow	= nullptr;
	m_magnitude		= 1.0f;
}

ui_actor_state_item::~ui_actor_state_item()
{
}

void ui_actor_state_item::init_from_xml( CUIXml& xml, LPCSTR path )
{
	CUIXmlInit::InitWindow( xml, path, 0, this);

	XML_NODE* stored_root = xml.GetLocalRoot();
	XML_NODE* new_root = xml.NavigateToNode( path, 0 );
	xml.SetLocalRoot( new_root );

	LPCSTR hint_text = xml.Read( "hint_text", 0, "no hint" );
	set_hint_text_ST( hint_text );
	
	set_hint_delay( (u32)xml.ReadAttribInt( "hint_text", 0, "delay" ) );

	if ( xml.NavigateToNode( "state_progress", 0 ) )	
	{
		m_progress = UIHelper::CreateProgressBar( xml, "state_progress", this );
		m_progress->IsExpressionSystem = xml.ReadAttrib(path, 0, "expression", nullptr) != nullptr;
	}
	if ( xml.NavigateToNode( "progress_shape", 0 ) )	
	{
		m_sensor = new CUIProgressShape();
		AttachChild( m_sensor );
		m_sensor->SetAutoDelete( true );
		CUIXmlInit::InitProgressShape( xml, "progress_shape", 0, m_sensor );
	}
	if ( xml.NavigateToNode( "arrow", 0 ) )	
	{
		m_arrow = new CUIArrow();
		m_arrow->init_from_xml( xml, "arrow", this );
	}
	if ( xml.NavigateToNode( "arrow_shadow", 0 ) )	
	{
		m_arrow_shadow = new CUIArrow();
		m_arrow_shadow->init_from_xml( xml, "arrow_shadow", this );
	}
	if ( xml.NavigateToNode( "icon", 0 ) )	
	{
		m_static = UIHelper::CreateStatic( xml, "icon", this );
		m_magnitude = xml.ReadAttribFlt( "icon", 0, "magnitude", 1.0f );
		m_static->TextItemControl()->SetText("");
	}
	if ( xml.NavigateToNode( "icon2", 0 ) )	
	{
		m_static2 = UIHelper::CreateStatic( xml, "icon2", this );
		m_magnitude = xml.ReadAttribFlt("icon2", 0, "magnitude", 1.0f);
		m_static2->TextItemControl()->SetText("");
	}
	if ( xml.NavigateToNode( "icon3", 0 ) )	
	{
		m_static3 = UIHelper::CreateStatic( xml, "icon3", this );
		m_magnitude = xml.ReadAttribFlt("icon3", 0, "magnitude", 1.0f);
		m_static3->TextItemControl()->SetText("");
	}
	set_arrow( 0.0f );
	xml.SetLocalRoot( stored_root );
}


bool ui_actor_state_item::set_text( float value )
{
	if (!m_static)
	{
		return false;
	}

	int v = (int)( value * m_magnitude + 0.49f );// m_magnitude=100
	clamp( v, 0, 99 );
	string32 text_res;
	xr_sprintf( text_res, sizeof(text_res), "%d", v );
	m_static->TextItemControl()->SetText( text_res );
	return true;
}

bool ui_actor_state_item::set_text_str( LPCSTR text )
{
	if (!m_static)
	{
		return false;
	}

	m_static->TextItemControl()->SetText( text );
	return true;
}

bool ui_actor_state_item::set_progress( float value )
{
	if ( !m_progress )
	{
		return false;
	}
	m_progress->SetProgressPos( value );
	return true;
}

bool ui_actor_state_item::set_progress_shape( float value )
{
	if ( !m_sensor )
	{
		return false;
	}
	m_sensor->SetPos( value );
	return true;
}

int ui_actor_state_item::set_arrow( float value )
{
	if ( !m_arrow )
	{
		return 0;	
	}
	m_arrow->SetNewValue( value );
	if ( !m_arrow_shadow )
	{
		return 1;
	}
	m_arrow_shadow->SetPos( m_arrow->GetPos() );
	return 2;
}


bool ui_actor_state_item::show_static( bool status, u8 number )
{
	switch(number)
	{
	case 1:
		if(!m_static)
			return false;
		m_static->Show(status);
		break;
	case 2:
		if(!m_static2)
			return false;
		m_static2->Show(status);
		break;
	case 3:
		if(!m_static3)
			return false;
		m_static3->Show(status);
		break;
	default:
		return false;
	}
	return true;
}
