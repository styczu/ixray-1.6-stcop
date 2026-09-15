////////////////////////////////////////////////////////////////////////////
//	Module 		: UIActorStateInfo.cpp
//	Created 	: 15.02.2008
//	Author		: Evgeniy Sokolov
//	Description : UI actor state window class implementation
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "UIActorStateInfo.h"
#include "UIConditionFormat.h"
#include "../ProtectionValues.h"
#include "../../xrUI/Widgets/UIProgressBar.h"
#include "../../xrUI/Widgets/UIProgressShape.h"
#include "../../xrUI/Widgets/UIScrollView.h"
#include "../../xrUI/Widgets/UIFrameWindow.h"
#include "../../xrUI/Widgets/UIStatic.h"
#include "../../xrUI/Widgets/UIStackPanel.h"
#include "../../xrUI/UIXmlInit.h"
#include "../../xrUI/UIFontDefines.h"
#include "../../xrUI/uiabstract.h"
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
#include "../../xrCore/EngineExternal.h"

namespace
{
Fvector2 ParseSize2(LPCSTR value, Fvector2 def)
{
	Fvector2 result = def;
	if (!value || !value[0])
	{
		return result;
	}

	float x = def.x;
	float y = def.y;
	const int count = sscanf(value, "%f,%f", &x, &y);
	if (count == 1)
	{
		result.set(x, x);
	}
	else if (count >= 2)
	{
		result.set(x, y);
	}
	return result;
}

u32 ParseColor4(LPCSTR value, u32 def)
{
	if (!value || !value[0])
	{
		return def;
	}

	if (!strchr(value, ','))
	{
		const auto* defs = CUIXmlInit::GetColorDefs();
		const auto it = defs->find(shared_str(value));
		if (it != defs->end())
		{
			return it->second;
		}
		return def;
	}

	int r = color_get_R(def);
	int g = color_get_G(def);
	int b = color_get_B(def);
	int a = color_get_A(def);
	const int count = sscanf(value, "%d,%d,%d,%d", &r, &g, &b, &a);
	if (count >= 3)
	{
		return color_argb(a, r, g, b);
	}
	return def;
}

CGameFont* ResolveFont(LPCSTR fontName)
{
	if (fontName && fontName[0])
	{
		if (CGameFont* font = UI().Font().GetFont(fontName))
		{
			return font;
		}
	}
	return UI().Font().GetFont(LETTERICA16_FONT_NAME);
}

ETextAlignment ParseTextAlign(LPCSTR value, ETextAlignment def)
{
	if (!value || !value[0])
	{
		return def;
	}
	if (0 == xr_strcmp(value, "left")) return CGameFont::alLeft;
	if (0 == xr_strcmp(value, "center") || 0 == xr_strcmp(value, "centre")) return CGameFont::alCenter;
	if (0 == xr_strcmp(value, "right")) return CGameFont::alRight;
	return def;
}

enum class EStateRowSlot : u8
{
	Icon,
	Caption,
	Value
};

struct StateRowSlot
{
	EStateRowSlot type = EStateRowSlot::Caption;
	float width = -1.f;
	bool autoIconWidth = false;
};

bool ParseStateRowSlotType(LPCSTR name, EStateRowSlot& out)
{
	if (!name || !name[0])
	{
		return false;
	}
	if (0 == xr_strcmp(name, "icon")) { out = EStateRowSlot::Icon; return true; }
	if (0 == xr_strcmp(name, "caption") || 0 == xr_strcmp(name, "text")) { out = EStateRowSlot::Caption; return true; }
	if (0 == xr_strcmp(name, "value") || 0 == xr_strcmp(name, "number") || 0 == xr_strcmp(name, "percent")) { out = EStateRowSlot::Value; return true; }
	return false;
}

void ParseStateRowSlots(LPCSTR columns, LPCSTR layout, float valueWidth, float iconWidth, xr_vector<StateRowSlot>& out)
{
	out.clear();
	LPCSTR source = (columns && columns[0]) ? columns : ((layout && layout[0]) ? layout : "icon,caption,value");
	const int count = _GetItemCount(source);
	for (int i = 0; i < count; ++i)
	{
		string64 token;
		_GetItem(source, i, token);
		if (!token[0])
		{
			continue;
		}

		string64 name;
		string64 sizeToken;
		name[0] = 0;
		sizeToken[0] = 0;
		if (char* colon = strchr(token, ':'))
		{
			*colon = 0;
			xr_strcpy(name, token);
			xr_strcpy(sizeToken, colon + 1);
		}
		else
		{
			xr_strcpy(name, token);
		}

		EStateRowSlot type;
		if (!ParseStateRowSlotType(name, type))
		{
			continue;
		}

		bool duplicate = false;
		for (const StateRowSlot& existing : out)
		{
			if (existing.type == type)
			{
				duplicate = true;
				break;
			}
		}
		if (duplicate)
		{
			continue;
		}

		StateRowSlot slot;
		slot.type = type;
		if (sizeToken[0])
		{
			if (0 == xr_strcmp(sizeToken, "*") || 0 == xr_strcmp(sizeToken, "flex"))
			{
				slot.width = -1.f;
			}
			else if (0 == xr_strcmp(sizeToken, "auto"))
			{
				if (type == EStateRowSlot::Icon)
				{
					slot.autoIconWidth = true;
					slot.width = iconWidth;
				}
				else if (type == EStateRowSlot::Value)
				{
					slot.width = valueWidth;
				}
				else
				{
					slot.width = -1.f;
				}
			}
			else
			{
				slot.width = float(atof(sizeToken));
			}
		}
		else
		{
			if (type == EStateRowSlot::Icon)
			{
				slot.autoIconWidth = true;
				slot.width = iconWidth;
			}
			else if (type == EStateRowSlot::Value)
			{
				slot.width = valueWidth;
			}
			else
			{
				slot.width = -1.f;
			}
		}
		out.push_back(slot);
	}

	if (out.empty())
	{
		StateRowSlot icon;
		icon.type = EStateRowSlot::Icon;
		icon.autoIconWidth = true;
		icon.width = iconWidth;
		out.push_back(icon);

		StateRowSlot caption;
		caption.type = EStateRowSlot::Caption;
		caption.width = -1.f;
		out.push_back(caption);

		StateRowSlot value;
		value.type = EStateRowSlot::Value;
		value.width = valueWidth;
		out.push_back(value);
	}
}

bool ReadOptionalFloatAttr(tinyxml2::XMLElement* element, LPCSTR name, float& out)
{
	if (!element || !name || !element->Attribute(name))
	{
		return false;
	}
	out = element->FloatAttribute(name);
	return true;
}

bool ResolveOptionalFloat(tinyxml2::XMLElement* element, LPCSTR name, bool hasDefault, float defaultValue, float& out)
{
	if (ReadOptionalFloatAttr(element, name, out))
	{
		return true;
	}
	if (hasDefault)
	{
		out = defaultValue;
		return true;
	}
	return false;
}

struct OptionalFloat
{
	bool set = false;
	float value = 0.f;
};

OptionalFloat ReadOptionalFloatFromXml(CUIXml& xml, LPCSTR path, LPCSTR attrib)
{
	OptionalFloat result;
	LPCSTR raw = xml.ReadAttrib(path, 0, attrib, nullptr);
	if (raw && raw[0])
	{
		result.set = true;
		result.value = xml.ReadAttribFlt(path, 0, attrib, 0.f);
	}
	return result;
}
}
#include "../BoneProtections.h"
#include "../../xrEngine/bone.h"
#include "../../xrEngine/device.h"
#include "../../Include/xrRender/Kinematics.h"
#include "../../xrEngine/string_table.h"

// Nazwa klasy pancerza dla progu przebicia. Tabela siedzi w danych:
//   [ui_armor_classes]  thresholds = 0.00, 0.21, ...   names = 0, 0a, 1, ...
// Progi rosnaco. Brak sekcji / wartosc ponizej pierwszego progu = nullptr.
// Wyprowadzone z anonimowej przestrzeni, bo korzysta z tego rowniez tooltip
// przedmiotu (klasa pancerza kombinezonu/helmu) - deklaracja w ProtectionValues.h.
LPCSTR Protection::ArmorClassName(float value)
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
	const u32 count = std::min((u32)s_thresholds.size(), (u32)s_names.size());
	for (u32 i = 0; i < count; ++i)
	{
		if (value + EPS_L >= s_thresholds[i])
			result = s_names[i].c_str();
	}
	return result;
}

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

	// ArmorClassName wyprowadzono do Protection:: (definicja nad ta przestrzenia).

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
					best = std::max(best, a * outfit->GetCondition());
			}
			if (helmet)
			{
				const float a = helmet->GetBoneArmor((s16)bone_id);
				if (a >= 0.0f)
					best = std::max(best, a * helmet->GetCondition());
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
	static LPCSTR kColComponent = "%c[255,154,160,164]";
	static LPCSTR kColValue = "%c[255,232,178,84]";
	static LPCSTR kColUnit  = "%c[255,132,140,144]";
	static LPCSTR kColClass = "%c[255,200,208,212]";
	static LPCSTR kColGain  = "%c[255,110,190,115]";
	static LPCSTR kColLoss  = "%c[255,210,50,50]";

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
    delete_data(m_environment_hint_wnd);
}

void ui_actor_state_wnd::init_from_xml( CUIXml& xml, const char* path )
{
	XML_NODE* stored_root = xml.GetLocalRoot();
	CUIXmlInit::InitWindow( xml, path, 0, this );

	XML_NODE* new_root = xml.NavigateToNode( path, 0 );
	xml.SetLocalRoot( new_root );

	if (xml.NavigateToNode("hint_wnd", 0))
	{
		m_hint_wnd = UIHelper::CreateHint( xml, "hint_wnd" );
	}
	if (xml.NavigateToNode("environment_hint_wnd", 0))
	{
		m_environment_hint_wnd = UIHelper::CreateHint( xml, "environment_hint_wnd" );
	}

	if (xml.NavigateToNode("state_list", 0))
	{
		m_listMode = true;
		init_list_from_xml(xml);
		xml.SetLocalRoot( stored_root );
		return;
	}

	m_listMode = false;
	init_legacy_from_xml(xml);
	xml.SetLocalRoot( stored_root );
}

void ui_actor_state_wnd::init_legacy_from_xml(CUIXml& xml)
{
	for ( int i = 0; i < stt_count; ++i )
	{
		m_state[i] = new ui_actor_state_item();
		m_state[i]->SetAutoDelete( true );
		AttachChild( m_state[i] );
		m_state[i]->set_hint_wnd( m_hint_wnd );
	}
    if (m_environment_hint_wnd)
        for (const auto state : {stt_fire, stt_shock, stt_acid, stt_radia, stt_psi})
            m_state[state]->set_hint_wnd(m_environment_hint_wnd);
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

	if (xml.NavigateToNode("intoxication_state"))
		m_state[stt_intoxication]->init_from_xml(xml, "intoxication_state");
}

void ui_actor_state_wnd::init_list_from_xml(CUIXml& xml)
{
	m_stateList = UIHelper::CreateStackPanel(xml, "state_list", this, true);
	if (!m_stateList)
	{
		return;
	}

	XML_NODE* listNode = xml.NavigateToNode("state_list", 0);
	const float listWidth = m_stateList->GetWidth();

	ui_actor_state_row::LayoutDefaults defaults;
	defaults.rowHeight = xml.ReadAttribFlt("state_list", 0, "row_height", 20.f);
	defaults.iconSize = ParseSize2(xml.ReadAttrib("state_list", 0, "icon_size", "18,18"), Fvector2().set(18.f, 18.f));
	defaults.iconColor = ParseColor4(xml.ReadAttrib("state_list", 0, "icon_color", nullptr), color_argb(255, 240, 140, 40));
	defaults.captionColor = ParseColor4(xml.ReadAttrib("state_list", 0, "caption_color", nullptr), color_argb(255, 200, 205, 210));
	defaults.valueColor = ParseColor4(xml.ReadAttrib("state_list", 0, "value_color", nullptr), color_argb(255, 200, 205, 210));
	defaults.magnitude = xml.ReadAttribFlt("state_list", 0, "value_magnitude", 100.f);
	defaults.format = xml.ReadAttrib("state_list", 0, "value_format", "%d%%");
	defaults.font = xml.ReadAttrib("state_list", 0, "font", LETTERICA16_FONT_NAME);
	defaults.layout = xml.ReadAttrib("state_list", 0, "layout", "icon,caption,value");
	defaults.columns = xml.ReadAttrib("state_list", 0, "columns", nullptr);
	defaults.pad = xml.ReadAttribFlt("state_list", 0, "pad", 4.f);
	defaults.valueWidth = xml.ReadAttribFlt("state_list", 0, "value_width", 42.f);
	defaults.captionAlign = xml.ReadAttrib("state_list", 0, "caption_align", "left");
	defaults.valueAlign = xml.ReadAttrib("state_list", 0, "value_align", "right");

	const OptionalFloat listCaptionX = ReadOptionalFloatFromXml(xml, "state_list", "caption_x");
	defaults.hasCaptionX = listCaptionX.set;
	defaults.captionX = listCaptionX.value;
	const OptionalFloat listCaptionY = ReadOptionalFloatFromXml(xml, "state_list", "caption_y");
	defaults.hasCaptionY = listCaptionY.set;
	defaults.captionY = listCaptionY.value;
	const OptionalFloat listValueX = ReadOptionalFloatFromXml(xml, "state_list", "value_x");
	defaults.hasValueX = listValueX.set;
	defaults.valueX = listValueX.value;
	const OptionalFloat listValueY = ReadOptionalFloatFromXml(xml, "state_list", "value_y");
	defaults.hasValueY = listValueY.set;
	defaults.valueY = listValueY.value;
	const OptionalFloat listIconX = ReadOptionalFloatFromXml(xml, "state_list", "icon_x");
	defaults.hasIconX = listIconX.set;
	defaults.iconX = listIconX.value;
	const OptionalFloat listIconY = ReadOptionalFloatFromXml(xml, "state_list", "icon_y");
	defaults.hasIconY = listIconY.set;
	defaults.iconY = listIconY.value;

	XML_NODE* storedRoot = xml.GetLocalRoot();
	xml.SetLocalRoot(listNode);

	for (tinyxml2::XMLElement* child = listNode ? listNode->FirstChildElement() : nullptr;
		child;
		child = child->NextSiblingElement())
	{
		LPCSTR tag = child->Name();
		if (!tag)
		{
			continue;
		}

		if (0 == xr_strcmp(tag, "item"))
		{
			auto* row = new ui_actor_state_row();
			row->SetAutoDelete(true);
			m_stateList->AttachChild(row);
			row->init_from_xml(xml, child, listWidth, defaults, m_hint_wnd);
			if (row->type() != stt_invalid)
			{
				m_listRows.push_back(row);
			}
		}
		else if (0 == xr_strcmp(tag, "separator"))
		{
			auto* separator = new CUIStatic();
			separator->SetAutoDelete(true);
			m_stateList->AttachChild(separator);

			const float height = child->FloatAttribute("height", 8.f);
			separator->SetWndSize(Fvector2().set(listWidth, height));
			separator->SetWndPos(Fvector2().set(0.f, 0.f));

			LPCSTR texture = child->Attribute("texture");
			if (!texture || !texture[0])
			{
				texture = "ui_inGame2_inventory_progress_bar";
			}
			separator->InitTexture(texture);
			separator->SetStretchTexture(true);
			separator->SetTextureColor(ParseColor4(child->Attribute("color"), color_argb(180, 120, 130, 140)));
		}
	}

	xml.SetLocalRoot(storedRoot);
}

ui_actor_state_wnd::EStateType ui_actor_state_wnd::ParseStateType(LPCSTR name)
{
	if (!name || !name[0])
	{
		return stt_invalid;
	}
	if (0 == xr_strcmp(name, "stamina")) return stt_stamina;
	if (0 == xr_strcmp(name, "health")) return stt_health;
	if (0 == xr_strcmp(name, "bleeding")) return stt_bleeding;
	if (0 == xr_strcmp(name, "radiation")) return stt_radiation;
	if (0 == xr_strcmp(name, "armor")) return stt_armor;
	if (0 == xr_strcmp(name, "main")) return stt_main;
	if (0 == xr_strcmp(name, "fire")) return stt_fire;
	if (0 == xr_strcmp(name, "radia")) return stt_radia;
	if (0 == xr_strcmp(name, "acid")) return stt_acid;
	if (0 == xr_strcmp(name, "psi")) return stt_psi;
	if (0 == xr_strcmp(name, "wound")) return stt_wound;
	if (0 == xr_strcmp(name, "fire_wound")) return stt_fire_wound;
	if (0 == xr_strcmp(name, "shock")) return stt_shock;
	if (0 == xr_strcmp(name, "power")) return stt_power;
	if (0 == xr_strcmp(name, "satiety") || 0 == xr_strcmp(name, "starvation")) return stt_satiety;
	if (0 == xr_strcmp(name, "thirst")) return stt_thirst;
	if (0 == xr_strcmp(name, "sleep") || 0 == xr_strcmp(name, "sleeping")) return stt_sleep;
	if (0 == xr_strcmp(name, "intoxication")) return stt_intoxication;
	return stt_invalid;
}

void ui_actor_state_row::init_from_xml(CUIXml& xml, XML_NODE* node, float rowWidth, const LayoutDefaults& defaults, UIHint* hintWnd)
{
	tinyxml2::XMLElement* element = node ? node->ToElement() : nullptr;
	if (!element)
	{
		return;
	}

	m_type = ui_actor_state_wnd::ParseStateType(element->Attribute("type"));
	const float rowHeight = element->FloatAttribute("row_height", defaults.rowHeight);
	SetWndSize(Fvector2().set(rowWidth, rowHeight));
	SetWndPos(Fvector2().set(0.f, 0.f));

	if (hintWnd)
	{
		set_hint_wnd(hintWnd);
	}

	LPCSTR hint = element->Attribute("hint_text");
	if (!hint || !hint[0])
	{
		hint = element->Attribute("caption");
	}
	if (hint && hint[0])
	{
		set_hint_text_ST(hint);
	}
	set_hint_delay(u32(element->IntAttribute("hint_delay", 800)));

	const Fvector2 iconSize = ParseSize2(element->Attribute("icon_size"), defaults.iconSize);
	const u32 iconColor = ParseColor4(element->Attribute("icon_color"), defaults.iconColor);
	const u32 captionColor = ParseColor4(element->Attribute("caption_color"), defaults.captionColor);
	const u32 valueColor = ParseColor4(element->Attribute("value_color"), defaults.valueColor);
	m_magnitude = element->FloatAttribute("value_magnitude", defaults.magnitude);
	LPCSTR format = element->Attribute("value_format");
	m_format = (format && format[0]) ? format : defaults.format;
	LPCSTR fontName = element->Attribute("font");
	CGameFont* font = ResolveFont((fontName && fontName[0]) ? fontName : defaults.font);

	const float pad = element->Attribute("pad") ? element->FloatAttribute("pad") : defaults.pad;
	const float valueWidth = element->Attribute("value_width") ? element->FloatAttribute("value_width") : defaults.valueWidth;
	LPCSTR layout = element->Attribute("layout");
	if (!layout || !layout[0])
	{
		layout = defaults.layout;
	}
	LPCSTR columns = element->Attribute("columns");
	if (!columns || !columns[0])
	{
		columns = defaults.columns;
	}
	const ETextAlignment captionAlign = ParseTextAlign(
		element->Attribute("caption_align") ? element->Attribute("caption_align") : defaults.captionAlign,
		CGameFont::alLeft);
	const ETextAlignment valueAlign = ParseTextAlign(
		element->Attribute("value_align") ? element->Attribute("value_align") : defaults.valueAlign,
		CGameFont::alRight);

	xr_vector<StateRowSlot> slots;
	ParseStateRowSlots(columns, layout, valueWidth, iconSize.x, slots);

	float fixedSum = 0.f;
	int flexCount = 0;
	for (StateRowSlot& slot : slots)
	{
		if (slot.type == EStateRowSlot::Icon && slot.autoIconWidth)
		{
			slot.width = iconSize.x;
		}
		if (slot.width < 0.f)
		{
			++flexCount;
		}
		else
		{
			fixedSum += slot.width;
		}
	}
	if (slots.size() > 1)
	{
		fixedSum += pad * float(slots.size() - 1);
	}
	const float flexWidth = (flexCount > 0) ? std::max(0.f, rowWidth - fixedSum) / float(flexCount) : 0.f;

	m_icon = new CUIStatic();
	m_icon->SetAutoDelete(true);
	AttachChild(m_icon);
	m_icon->Show(false);

	m_caption = new CUIStatic();
	m_caption->SetAutoDelete(true);
	AttachChild(m_caption);
	m_caption->Show(false);
	m_caption->TextItemControl()->SetFont(font);
	m_caption->SetTextColor(captionColor);
	m_caption->SetTextAlignment(captionAlign);
	m_caption->SetVTextAlignment(valCenter);
	LPCSTR caption = element->Attribute("caption");
	if (caption && caption[0])
	{
		m_caption->SetTextST(caption);
	}

	m_value = new CUIStatic();
	m_value->SetAutoDelete(true);
	AttachChild(m_value);
	m_value->Show(false);
	m_value->TextItemControl()->SetFont(font);
	m_value->SetTextColor(valueColor);
	m_value->SetTextAlignment(valueAlign);
	m_value->SetVTextAlignment(valCenter);
	m_value->TextItemControl()->SetText("0%");

	float cursorX = 0.f;
	for (const StateRowSlot& slot : slots)
	{
		const float width = (slot.width < 0.f) ? flexWidth : slot.width;
		float posX = cursorX;
		float posY = 0.f;
		float sizeX = width;
		float sizeY = rowHeight;

		if (slot.type == EStateRowSlot::Icon)
		{
			float absX = 0.f;
			float absY = 0.f;
			const bool hasAbsX = ResolveOptionalFloat(element, "icon_x", defaults.hasIconX, defaults.iconX, absX);
			if (hasAbsX)
			{
				posX = absX;
			}
			if (ResolveOptionalFloat(element, "icon_y", defaults.hasIconY, defaults.iconY, absY))
			{
				posY = absY;
			}
			else
			{
				posY = (rowHeight - iconSize.y) * 0.5f;
			}
			sizeX = iconSize.x;
			sizeY = iconSize.y;
			if (hasAbsX && element->Attribute("icon_width"))
			{
				sizeX = element->FloatAttribute("icon_width");
			}
			if (element->Attribute("icon_height"))
			{
				sizeY = element->FloatAttribute("icon_height");
			}

			m_icon->Show(true);
			m_icon->SetWndPos(Fvector2().set(posX, posY));
			m_icon->SetWndSize(Fvector2().set(sizeX, sizeY));
			LPCSTR iconTex = element->Attribute("icon");
			if (iconTex && iconTex[0])
			{
				m_icon->InitTexture(iconTex);
				m_icon->SetStretchTexture(true);
				m_icon->SetTextureColor(iconColor);
			}
		}
		else if (slot.type == EStateRowSlot::Caption)
		{
			float absX = 0.f;
			float absY = 0.f;
			const bool hasAbsX = ResolveOptionalFloat(element, "caption_x", defaults.hasCaptionX, defaults.captionX, absX);
			if (hasAbsX)
			{
				posX = absX;
			}
			if (ResolveOptionalFloat(element, "caption_y", defaults.hasCaptionY, defaults.captionY, absY))
			{
				posY = absY;
			}
			if (hasAbsX && element->Attribute("caption_width"))
			{
				sizeX = element->FloatAttribute("caption_width");
			}
			if (element->Attribute("caption_height"))
			{
				sizeY = element->FloatAttribute("caption_height");
			}

			m_caption->Show(true);
			m_caption->SetWndPos(Fvector2().set(posX, posY));
			m_caption->SetWndSize(Fvector2().set(sizeX, sizeY));
		}
		else
		{
			float absX = 0.f;
			float absY = 0.f;
			const bool hasAbsX = ResolveOptionalFloat(element, "value_x", defaults.hasValueX, defaults.valueX, absX);
			if (hasAbsX)
			{
				posX = absX;
			}
			if (ResolveOptionalFloat(element, "value_y", defaults.hasValueY, defaults.valueY, absY))
			{
				posY = absY;
			}
			if (hasAbsX && element->Attribute("value_width"))
			{
				sizeX = element->FloatAttribute("value_width");
			}
			if (element->Attribute("value_height"))
			{
				sizeY = element->FloatAttribute("value_height");
			}

			m_value->Show(true);
			m_value->SetWndPos(Fvector2().set(posX, posY));
			m_value->SetWndSize(Fvector2().set(sizeX, sizeY));
		}

		cursorX += width + pad;
	}
}

void ui_actor_state_row::set_value(float normalized)
{
	if (!m_value)
	{
		return;
	}

	float clamped = clampr(normalized, 0.f, 1.f);
	int v = iFloor(clamped * m_magnitude + 0.5f);
	clamp(v, 0, iFloor(m_magnitude + 0.5f));

	string64 text;
	xr_sprintf(text, sizeof(text), m_format.c_str() ? m_format.c_str() : "%d%%", v);
	m_value->TextItemControl()->SetText(text);
}

void ui_actor_state_wnd::SetListValue(EStateType type, float normalized)
{
	for (ui_actor_state_row* row : m_listRows)
	{
		if (row && row->type() == type)
		{
			row->set_value(normalized);
		}
	}
}

void ui_actor_state_wnd::UpdateActorInfo(CInventoryOwner* owner)
{
	CActor* actor = owner->cast_actor();
	if (actor == nullptr)
	{
		return;
	}

	if (m_listMode)
	{
		UpdateActorInfoList(actor);
		return;
	}

	UpdateActorInfoLegacy(actor);
}

void ui_actor_state_wnd::UpdateActorInfoList(CActor* actor)
{
	SetListValue(stt_health, actor->conditions().GetHealth());
	SetListValue(stt_thirst, actor->conditions().GetThirst());
	SetListValue(stt_satiety, actor->conditions().GetSatiety());
	SetListValue(stt_sleep, actor->conditions().GetSleepiness());
	SetListValue(stt_radiation, actor->conditions().GetRadiation());

	const static bool enableMedIntoxication = EngineExternal()[EEngineExternalGame::EnableMedIntoxication];
	SetListValue(stt_intoxication, enableMedIntoxication ? actor->conditions().GetIntoxication() : 0.f);

	float stamina = actor->GetRestoreSpeed(ALife::ePowerRestoreSpeed);
	SetListValue(stt_stamina, clampr(stamina, 0.f, 1.f));

	CCustomOutfit* outfit = actor->GetOutfit();
	CHelmet* helmet = actor->GetHelmet();

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

	if (outfit)
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
		float boneArmor = outfit->GetBoneArmor(spine_bone);
		SetListValue(stt_armor, clampr(boneArmor, 0.f, 1.f));

		fwou_value += boneArmor * outfit->GetCondition();
		if (!outfit->bIsHelmetAvaliable)
		{
			u16 head_bone = ikv->LL_BoneID("bip01_head");
			fwou_value += outfit->GetBoneArmor(head_bone) * outfit->GetCondition();
		}
	}
	else
	{
		SetListValue(stt_armor, 0.f);
	}

	if (helmet)
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
		fwou_value += helmet->GetBoneArmor(spine_bone) * helmet->GetCondition();
	}

	const auto getProtection = [&](float& valueRef, ALife::EHitType hitType) -> float
	{
		valueRef += actor->GetProtection_ArtefactsOnBelt(hitType);
		return actor->conditions().GetZoneMaxPower(hitType);
	};

	auto setProtection = [&](EStateType type, float value, ALife::EHitType hitType)
	{
		const float max_power = getProtection(value, hitType);
		const float norm = (max_power > EPS) ? clampr(value / max_power, 0.f, 1.f) : 0.f;
		SetListValue(type, norm);
	};

	setProtection(stt_fire, burn_value, ALife::eHitTypeBurn);
	setProtection(stt_radia, radi_value, ALife::eHitTypeRadiation);
	setProtection(stt_acid, cmbn_value, ALife::eHitTypeChemicalBurn);
	setProtection(stt_psi, tele_value, ALife::eHitTypeTelepatic);
	setProtection(stt_wound, woun_value, ALife::eHitTypeWound);
	setProtection(stt_shock, shoc_value, ALife::eHitTypeShock);
	setProtection(stt_fire_wound, fwou_value, ALife::eHitTypeFireWound);

	const float maxPowerRestore = actor->conditions().GetMaxPowerRestoreSpeed();
	float powerNorm = 0.f;
	if (maxPowerRestore > EPS)
	{
		powerNorm = clampr(actor->GetRestoreSpeed(ALife::ePowerRestoreSpeed) / maxPowerRestore, 0.f, 1.f);
	}
	SetListValue(stt_power, powerNorm);
}

void ui_actor_state_wnd::UpdateActorInfoLegacy(CActor* actor)
{
	float value = 0.0f;

	if (!m_state[stt_health]->m_progress->IsExpressionSystem)
	{
		value = actor->conditions().GetHealth();
		value = floor(value * 55) / 55;
		m_state[stt_health]->set_progress(value);
	}

	if (m_state[stt_thirst]->m_progress != nullptr && !m_state[stt_thirst]->m_progress->IsExpressionSystem)
	{
		value = actor->conditions().GetThirst();
		m_state[stt_thirst]->set_progress(value);
	}
	
	if (m_state[stt_satiety]->m_progress != nullptr && !m_state[stt_satiety]->m_progress->IsExpressionSystem)
	{
		const static bool enableMedIntoxication = EngineExternal()[EEngineExternalGame::EnableMedIntoxication];
		value = enableMedIntoxication
			? actor->conditions().GetIntoxication()
			: actor->conditions().GetSatiety();
		m_state[stt_satiety]->set_progress(value);
	}
	
	if (m_state[stt_sleep]->m_progress != nullptr && !m_state[stt_sleep]->m_progress->IsExpressionSystem)
	{
		value = actor->conditions().GetSleepiness();
		m_state[stt_sleep]->set_progress(value);
	}

	value = actor->GetRestoreSpeed(ALife::ePowerRestoreSpeed);
	m_state[stt_stamina]->set_text(value);

	value = actor->conditions().BleedingSpeed();
	m_state[stt_health]->show_static((value > 0.01f));
	
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

	const static bool enableMedIntoxication = EngineExternal()[EEngineExternalGame::EnableMedIntoxication];
	if (enableMedIntoxication)
	{
		if (m_state[stt_intoxication]->m_progress != nullptr && !m_state[stt_intoxication]->m_progress->IsExpressionSystem)
		{
			value = actor->conditions().GetIntoxication();
			m_state[stt_intoxication]->set_progress(value);
		}

		value = actor->conditions().GetIntoxication();
		m_state[stt_intoxication]->show_static(false, 1);
		m_state[stt_intoxication]->show_static(false, 2);
		m_state[stt_intoxication]->show_static(false, 3);
		if (!fis_zero(value, EPS))
		{
			if (value < 0.35f)
				m_state[stt_intoxication]->show_static(true, 1);
			else if (value < 0.7f)
				m_state[stt_intoxication]->show_static(true, 2);
			else
				m_state[stt_intoxication]->show_static(true, 3);
		}
	}

	CCustomOutfit* outfit = actor->GetOutfit();
	CHelmet* helmet = actor->GetHelmet();

	m_state[stt_fire_wound]->set_progress(0.0f);
	m_state[stt_fire]->set_progress(0.0f);
	m_state[stt_radia]->set_progress(0.0f);
	m_state[stt_acid]->set_progress(0.0f);
	m_state[stt_psi]->set_progress(0.0f);
	m_state[stt_wound]->set_progress(0.0f);
	m_state[stt_shock]->set_progress(0.0f);
	m_state[stt_power]->set_progress(0.0f);

	// Rozszarpanie/uderzenie/wybuch i balistyka licza sie w UpdateCombatProtection
	// (ponizej). Dawna sciezka fwou_value/woun_value z pancerza kosci usunieta -
	// byla niewidoczna (pasek wiersza szedl z wyrazenia XML).

	{
		const float max_power = actor->conditions().GetZoneMaxPower(ALife::eHitTypeBurn);
		update_round_states(stt_fire, actor->GetEquipmentProtection(ALife::eHitTypeBurn), max_power);
	}
	{
		const float max_power = actor->conditions().GetZoneMaxPower(ALife::eHitTypeRadiation);
		update_round_states(stt_radia, actor->GetEquipmentProtection(ALife::eHitTypeRadiation), max_power);
	}
	{
		const float max_power = actor->conditions().GetZoneMaxPower(ALife::eHitTypeChemicalBurn);
		update_round_states(stt_acid, actor->GetEquipmentProtection(ALife::eHitTypeChemicalBurn), max_power);
	}
	{
		const float max_power = actor->conditions().GetZoneMaxPower(ALife::eHitTypeTelepatic);
		update_round_states(stt_psi, actor->GetEquipmentProtection(ALife::eHitTypeTelepatic), max_power);
	}
	{
		const float max_power = actor->conditions().GetZoneMaxPower(ALife::eHitTypeShock);
		update_round_states(stt_shock, actor->GetEquipmentProtection(ALife::eHitTypeShock), max_power);
	}
	{
		if (m_state[stt_power]->m_progress && !m_state[stt_power]->m_progress->IsExpressionSystem)
		{
			value = actor->GetRestoreSpeed(ALife::ePowerRestoreSpeed) / actor->conditions().GetMaxPowerRestoreSpeed();
			value = floor(value * 31) / 31;

			m_state[stt_power]->set_progress(value);
		}
	}

	UpdateArmorInfo( actor, outfit, helmet );
	UpdateCombatProtection( actor, outfit, helmet );
	UpdateRateHints( actor );
	UpdateProtectionHints( actor );

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
			LPCSTR cls = Protection::ArmorClassName(a);
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
// BleedingSpeed is the engine's aggregate intensity (average wound size),
// not a sum over bones. Scale it by 100 without changing damage or healing.
void ui_actor_state_wnd::UpdateBleedingInfo(CActor* actor)
{
    auto& condition = actor->conditions();
    const float bleeding = condition.BleedingSpeed();
    const float intensity = ConditionUi::BleedingIntensity(bleeding);
    const float timeFactor = ConditionUi::CurrentTimeFactor();
    const float drain = condition.CanBeHarmed() && !GodMode()
        ? ConditionUi::PercentPerSecond(bleeding * condition.change_v().m_fV_Bleeding, timeFactor) : 0.0f;
    // GetRestoreSpeed includes natural healing, belt artefacts (with condition)
    // and the worn outfit; temporary medicine is maintained separately.
    // This is healing strength per wound component, not d(intensity)/dt.
    const float reduction = ConditionUi::PercentPerSecond(
        actor->GetRestoreSpeed(ALife::eBleedingRestoreSpeed) + condition.GetBoostBleedingRestore(), timeFactor);

    xr_string hint = g_pStringTable->translate("ui_uip_tt_bleed").c_str();
    const auto line = [&](LPCSTR label, float value, bool fixed, LPCSTR suffix, LPCSTR color)
    {
        string32 number;
        if (fixed)
            FormatRate(number, value, 2);
        else
            ConditionUi::FormatNumber(number, value, ConditionUi::DecimalSeparator(), false);
        hint += kBreak;
        hint += kColLabel;
        hint += g_pStringTable->translate(label).c_str();
        hint += " ";
        hint += color;
        hint += number;
        if (suffix)
            hint += g_pStringTable->translate(suffix).c_str();
    };
    line("ui_uip_tt_bleed_level", intensity, true, nullptr, kColTitle);
    line("ui_uip_tt_bleed_drain", drain, false, "ui_uip_unit_hp_percent_s",
        drain > 0.0f ? "%c[255,210,80,65]" : kColTitle);
    line("ui_uip_tt_bleed_heal", reduction, false, "ui_uip_unit_bleed_s",
        reduction > 0.0f ? "%c[255,110,190,115]" : kColTitle);
    hint += kBreak;
    hint += kColSep;
    hint += g_pStringTable->translate("ui_armor_tt_sep").c_str();
    hint += kBreak;
    hint += kColLabel;
    hint += g_pStringTable->translate("ui_uip_tt_bleed_description").c_str();
    m_state[stt_bleeding]->set_hint_text(hint.c_str());
    m_state[stt_bleeding]->set_bleeding(intensity);
}

void ui_actor_state_wnd::UpdateProtectionHints(CActor* actor)
{
    const shared_str gate = g_pStringTable->translate("ui_uip_protection_effective");
    if (xr_strcmp(gate.c_str(), "ui_uip_protection_effective") == 0)
        return;
    CCustomOutfit* outfit = actor->GetOutfit();
    CHelmet* helmet = actor->GetHelmet();
    const auto update = [&](EStateType state, ALife::EHitType type)
    {
        if (!m_state[state])
            return;
        const float maximum = actor->conditions().GetZoneMaxPower(type);
        const float outfitThreshold = outfit ? Protection::EquipmentContribution(outfit->GetDefHitTypeProtection(type), type) : 0.0f;
        // HitOutfitEffect applies every equipped helmet, regardless of outfit UI flags.
        const float helmetThreshold = helmet ? Protection::EquipmentContribution(helmet->GetDefHitTypeProtection(type), type) : 0.0f;
        const float flatBoost = actor->conditions().GetEnvironmentalProtectionBoost(type);
        const float artifactMultiplier = actor->HitArtefactsOnBelt(1.0f, type);
        const auto effective = Protection::EffectiveThreshold(outfitThreshold, helmetThreshold, flatBoost, artifactMultiplier);
        const auto exposure = actor->GetEnvironmentalExposure(type);
        // The source layer and panel share the same raw-hit scale as the effective threshold.
        m_state[state]->set_environmental_exposure(type, Protection::DisplayRatio(exposure.power, maximum),
            Protection::DisplayRatio(actor->GetEquipmentProtection(type), maximum), exposure.opacity);
        xr_string hint = kColTitle;
        hint += g_pStringTable->translate(Protection::Caption(type)).c_str();
        const auto separator = [&]()
        {
            hint += kBreak;
            hint += kColSep;
            hint += ". . . . . . . . . . . . . .";
            hint += kBreak;
        };
        const auto line = [&](LPCSTR key, LPCSTR value, bool component)
        {
            hint += kBreak;
            hint += component ? kColComponent : kColLabel;
            if (component)
                hint += "  ";
            hint += g_pStringTable->translate(key).c_str();
            hint += ": ";
            if (!component)
                hint += kColTitle;
            hint += value;
        };
        // Components carry a sign; zero is printed as "0 pkt".
        const auto pointsLine = [&](LPCSTR key, float power, bool indent = false)
        {
            string64 value;
            ConditionUi::FormatProtectionPoints(value, Protection::DisplayRatio(power, maximum), indent, power != 0.0f);
            line(key, value, indent);
        };
        const auto thresholdLine = [&](LPCSTR key, const Protection::ThresholdReading& threshold)
        {
            if (threshold.attainable)
                pointsLine(key, threshold.power);
            else
                line(key, g_pStringTable->translate("ui_uip_protection_no_threshold").c_str(), false);
        };
        const auto percentLine = [&](LPCSTR key, float percent, bool indent)
        {
            string32 number;
            string64 value;
            ConditionUi::FormatDetailNumber(number, percent, ConditionUi::DecimalSeparator(), false);
            xr_strconcat(value, number, "%");
            line(key, value, indent);
        };
        const auto valueLine = [&](LPCSTR key, float amount, LPCSTR unitKey)
        {
            string32 number;
            string64 value;
            ConditionUi::FormatDetailNumber(number, amount, ConditionUi::DecimalSeparator(), false);
            xr_strconcat(value, number, " ", g_pStringTable->translate(unitKey).c_str());
            line(key, value, false);
        };

        separator();
        thresholdLine("ui_uip_protection_effective", effective);
        // The legacy burn/light_burn protection copies may diverge after upgrades.
        // Keep the measured thermal channel intact and expose a second threshold only when needed.
        if (type == ALife::eHitTypeBurn)
        {
            const auto lightType = ALife::eHitTypeLightBurn;
            const float lightOutfit = outfit ? Protection::EquipmentContribution(outfit->GetDefHitTypeProtection(lightType), lightType) : 0.0f;
            const float lightHelmet = helmet ? Protection::EquipmentContribution(helmet->GetDefHitTypeProtection(lightType), lightType) : 0.0f;
            const auto light = Protection::EffectiveThreshold(lightOutfit, lightHelmet, 0.0f, actor->HitArtefactsOnBelt(1.0f, lightType));
            if (light.attainable != effective.attainable || light.power != effective.power)
                thresholdLine("ui_uip_protection_effective_light_burn", light);
        }
        // Base threshold x artefact influence = effective threshold (nonnegative layers).
        hint += kBreak;
        pointsLine("ui_uip_protection_total", outfitThreshold + helmetThreshold + flatBoost);
        if (outfitThreshold != 0.0f)
            pointsLine("ui_uip_protection_outfit", outfitThreshold, true);
        if (helmetThreshold != 0.0f)
            pointsLine("ui_uip_protection_helmet", helmetThreshold, true);
        if (flatBoost != 0.0f)
            pointsLine("ui_uip_protection_temporary", flatBoost, true);
        {
            const float influence = Protection::ArtefactInfluence(artifactMultiplier);
            const int direction = Protection::InfluenceDirection(influence);
            string32 number;
            string64 value;
            ConditionUi::FormatNumber(number, influence, ConditionUi::DecimalSeparator(), false, 2);
            xr_strconcat(value, direction > 0 ? kColGain : (direction < 0 ? kColLoss : kColTitle), "x", number);
            line("ui_uip_protection_artifact_effect", value, false);
        }

        // Raw belt coefficients x100; the reduction comes from the game's own hit multiplier.
        hint += kBreak;
        separator();
        const float absorption = actor->GetProtection_ArtefactsOnBelt(type);
        {
            string32 number;
            string64 value;
            // Round first so a tiny malus never prints as "-0%".
            const float reduction = absorption != 0.0f ? std::round((1.0f - artifactMultiplier) * 1000.0f) / 10.0f : 0.0f;
            ConditionUi::FormatNumber(number, reduction, ConditionUi::DecimalSeparator(), false, 1);
            xr_strconcat(value, number, "%");
            line("ui_uip_protection_reduction", value, false);
        }
        if (absorption != 0.0f)
        {
            string64 value;
            ConditionUi::FormatAbsorption(value, absorption, false, true);
            line("ui_uip_protection_absorption", value, false);
            for (const PIItem item : actor->inventory().m_belt)
            {
                CArtefact* artefact = item->cast_artefact();
                const float coefficient = artefact ? artefact->m_ArtefactHitImmunities.AffectHit(1.0f, type) * artefact->GetCondition() : 0.0f;
                if (coefficient != 0.0f)
                {
                    ConditionUi::FormatAbsorption(value, coefficient, true, true);
                    hint += kBreak;
                    hint += kColComponent;
                    hint += "  ";
                    hint += artefact->NameItem();
                    hint += ": ";
                    hint += value;
                }
            }
        }

        // Same 750 ms peak as the source bar: do not flash zero between pulses.
        if (exposure.power > 0.0f)
        {
            hint += kBreak;
            separator();
            pointsLine("ui_uip_protection_source", exposure.power);
            const auto damage = actor->conditions().GetEnvironmentalDamage(type);
            if (type == ALife::eHitTypeRadiation)
                valueLine("ui_uip_protection_received_dose", damage.radiation * ConditionUi::RadiationScale, "ui_uip_unit_rad");
            else
                percentLine("ui_uip_protection_damage", (type == ALife::eHitTypeTelepatic ? damage.psy : damage.health) * 100.0f, false);
        }
        m_state[state]->set_hint_text(hint.c_str());
    };
    update(stt_fire, ALife::eHitTypeBurn);
    update(stt_shock, ALife::eHitTypeShock);
    update(stt_acid, ALife::eHitTypeChemicalBurn);
    update(stt_radia, ALife::eHitTypeRadiation);
    update(stt_psi, ALife::eHitTypeTelepatic);
}

// Ochrona bojowa - tylko parametry OCHRONNE (bez odczytu obrazen/sily zrodla).
// Mechaniczne (rozszarpanie/uderzenie/wybuch): prog absolutny odejmowany od
// trafienia (kombinezon+helm, kazdy *condition) plus nieliniowa redukcja
// artefaktow na pasie; "Efektywny prog" = prog / (1 - f). Skala pkt = wartosc
// x100, pelny pasek przy 1.00, trojkat powyzej. Balistyka: absorpcja pancerza
// (1 - hit_fraction_actor) plus dodatkowa redukcja z mnoznika trudnosci; osobno
// scenariusz zatrzymania i przebicia pancerza; skala w %.
void ui_actor_state_wnd::UpdateCombatProtection(CActor* actor, CCustomOutfit* outfit, CHelmet* helmet)
{
    // Panel (liczba/pasek/trojkat) liczy sie zawsze; bogaty tooltip wymaga
    // przetlumaczonych kluczy z dodatku (jak UpdateProtectionHints).
    const shared_str gate = g_pStringTable->translate("ui_uip_prot_threshold");
    const bool richTooltips = xr_strcmp(gate.c_str(), "ui_uip_prot_threshold") != 0;

    const bool showHelmet = helmet && (!outfit || outfit->bIsHelmetAvaliable);
    LPCSTR dotted = ". . . . . . . . . . . . . .";
    // Trzymamy shared_str, nie .c_str() z tymczasowego obiektu (patrz UpdateArmorInfo).
    const shared_str s_unit_pct = g_pStringTable->translate("ui_uip_unit_percent");
    LPCSTR unitPct = s_unit_pct.c_str();

    // Nieliniowy mnoznik redukcji artefaktow na pasie dla typu niestrefowego
    // (jak CActor::HitArtefactsOnBelt): dodatni = redukcja, ujemny = malus.
    auto artefactFactor = [&](ALife::EHitType type) -> float
    {
        float sum = actor->GetProtection_ArtefactsOnBelt(type);
        if (sum == 0.0f)
            return 0.0f;
        clamp(sum, -0.99f, 0.99f);
        return (sum > 0.0f)
            ? 1.5f * (float)pow(0.9f, (4.0f / sum))
            : -1.5f * (float)pow(0.9f, (4.0f / (-1.0f * sum)));
    };

    // ---- rozszarpanie / uderzenie / wybuch (prog absolutny, skala pkt) ----
    auto mechanical = [&](EStateType state, ALife::EHitType type, LPCSTR titleKey, LPCSTR effectKey)
    {
        ui_actor_state_item* item = m_state[state];
        if (!item)
            return;

        const float outfitProt = outfit ? outfit->GetDefHitTypeProtection(type) : 0.0f;
        const float helmetProt = showHelmet ? helmet->GetDefHitTypeProtection(type) : 0.0f;
        const float threshold  = outfitProt + helmetProt;             // Prog ochrony
        const float f          = artefactFactor(type);
        const float effective  = (f < 0.999f) ? threshold / (1.0f - f) : threshold; // Efektywny prog

        string64 num;
        ConditionUi::FormatProtectionPointsPlain(num, effective);     // x100, calkowite, bez jednostki
        item->set_value_text(num);
        if (item->m_progress)
        {
            float fill = effective;
            clamp(fill, 0.0f, 1.0f);
            item->m_progress->SetProgressPosImmediate(fill);
        }
        item->set_protection_overflow(effective);                     // trojkat gdy >100 pkt

        if (!richTooltips)
            return;

        xr_string hint = kColTitle;
        hint += g_pStringTable->translate(titleKey).c_str();
        hint += kBreak;
        hint += kColSep;
        hint += dotted;

        auto ptsLine = [&](LPCSTR key, float ratio, LPCSTR labelCol, LPCSTR valueCol, bool indent)
        {
            string64 v;
            ConditionUi::FormatProtectionPoints(v, ratio, false, true);     // "X pkt"
            hint += kBreak;
            hint += labelCol;
            if (indent)
                hint += "  ";
            hint += g_pStringTable->translate(key).c_str();
            hint += ": ";
            hint += valueCol;
            hint += v;
        };

        ptsLine("ui_uip_prot_effective_threshold", effective, kColLabel, kColTitle, false);
        ptsLine("ui_uip_prot_threshold", threshold, kColLabel, kColTitle, false);
        if (outfit && outfitProt > 0.0f)
            ptsLine("ui_uip_protection_outfit", outfitProt, kColComponent, kColComponent, true);
        if (showHelmet && helmetProt > 0.0f)
            ptsLine("ui_uip_protection_helmet", helmetProt, kColComponent, kColComponent, true);

        // Wplyw artefaktow - tylko gdy jakis artefakt na pasie dotyczy tego typu.
        if (f != 0.0f)
        {
            string64 pct;
            ConditionUi::FormatDetailNumber(pct, f * 100.0f, ConditionUi::DecimalSeparator(), false, 2);
            hint += kBreak;
            hint += kColLabel;
            hint += g_pStringTable->translate("ui_uip_prot_artefact_influence").c_str();
            hint += ": ";
            hint += kColTitle;
            hint += pct;
            hint += unitPct;

            for (const PIItem beltItem : actor->inventory().m_belt)
            {
                CArtefact* af = beltItem ? beltItem->cast_artefact() : nullptr;
                if (!af)
                    continue;
                const float imm = af->m_ArtefactHitImmunities.AffectHit(1.0f, type); // surowe *_immunity
                if (imm == 0.0f)
                    continue;
                string64 iv;
                ConditionUi::FormatDetailNumber(iv, imm * 100.0f, ConditionUi::DecimalSeparator(), false, 2);
                hint += kBreak;
                hint += kColComponent;
                hint += "  ";
                hint += beltItem->NameShort();
                hint += ": ";
                hint += kColComponent;
                hint += iv;
                hint += unitPct;
            }
        }

        // Skutek trafienia - statyczny opis typu (nie liczba obrazen).
        hint += kBreak;
        hint += kColSep;
        hint += dotted;
        hint += kBreak;
        hint += kColLabel;
        hint += g_pStringTable->translate("ui_uip_prot_hit_effect").c_str();
        hint += " ";
        hint += kColTitle;
        hint += g_pStringTable->translate(effectKey).c_str();

        item->set_hint_text(hint.c_str());
    };

    mechanical(stt_wound, ALife::eHitTypeWound,     "ui_uip_prot_wound_title",  "ui_uip_prot_effect_wound");
    mechanical(stt_main,  ALife::eHitTypeStrike,    "ui_uip_prot_strike_title", "ui_uip_prot_effect_strike");
    mechanical(stt_sleep, ALife::eHitTypeExplosion, "ui_uip_prot_explo_title",  "ui_uip_prot_effect_explo");

    // ---- balistyka (absorpcja pancerza + mnoznik trudnosci, skala %) ----
    {
        ui_actor_state_item* item = m_state[stt_fire_wound];
        if (item)
        {
            // Mnoznik OBRAZEN z poziomu trudnosci (actor_immunities_<diff>).
            // Efekty czasowe (m_fBoost*Immunity) sa zerowe w bazowej grze i tu pominiete.
            const float mult = actor->conditions().GetHitImmunity(ALife::eHitTypeFireWound);
            if (!outfit)
            {
                item->set_value_text("-");
                if (item->m_progress)
                    item->m_progress->SetProgressPosImmediate(0.0f);
            }
            else
            {
                const float hitFraction = outfit->GetHitFractionActor(); // czesc przechodzaca
                const float effStopped  = 1.0f - hitFraction * mult;     // Efektywna ochrona (zatrzymany)
                string64 num;
                ConditionUi::FormatProtectionPointsPlain(num, effStopped);
                xr_strcat(num, unitPct);
                item->set_value_text(num);
                if (item->m_progress)
                {
                    float fill = effStopped;
                    clamp(fill, 0.0f, 1.0f);
                    item->m_progress->SetProgressPosImmediate(fill);
                }
            }
            item->set_protection_overflow(0.0f); // balistyka nie przekracza 100%

            if (richTooltips && outfit)
            {
                const float hitFraction = outfit->GetHitFractionActor();
                const float absorb      = 1.0f - hitFraction;          // Absorpcja pancerza
                const float redStopped  = hitFraction * (1.0f - mult); // Dodatkowa redukcja (trudnosc)
                const float effStopped  = absorb + redStopped;         // Efektywna ochrona
                const float redPen      = 1.0f - mult;                 // przy przebiciu (absorpcja=0)

                xr_string hint = kColTitle;
                hint += g_pStringTable->translate("ui_uip_prot_ballistic_title").c_str();
                hint += kBreak;
                hint += kColSep;
                hint += dotted;

                auto pctLine = [&](LPCSTR key, float frac, LPCSTR labelCol, LPCSTR valueCol, bool indent)
                {
                    string64 v;
                    ConditionUi::FormatDetailNumber(v, frac * 100.0f, ConditionUi::DecimalSeparator(), false, 2);
                    hint += kBreak;
                    hint += labelCol;
                    if (indent)
                        hint += "  ";
                    hint += g_pStringTable->translate(key).c_str();
                    hint += ": ";
                    hint += valueCol;
                    hint += v;
                    hint += unitPct;
                };

                // Po zatrzymaniu pocisku
                pctLine("ui_uip_prot_effective", effStopped, kColLabel, kColTitle, false);
                pctLine("ui_uip_prot_armor_absorption", absorb, kColComponent, kColComponent, true);
                pctLine("ui_uip_prot_difficulty_reduction", redStopped, kColComponent, kColComponent, true);
                hint += kBreak;
                hint += kColLabel;
                hint += g_pStringTable->translate("ui_uip_prot_bleeding").c_str();
                hint += " ";
                hint += kColTitle;
                hint += g_pStringTable->translate("ui_uip_prot_bleeding_no").c_str();

                // Przy przebiciu pancerza (hit_fraction pominiete)
                hint += kBreak;
                hint += kColSep;
                hint += dotted;
                hint += kBreak;
                hint += kColTitle;
                hint += g_pStringTable->translate("ui_uip_prot_penetration_header").c_str();
                pctLine("ui_uip_prot_effective", redPen, kColLabel, kColTitle, false);
                pctLine("ui_uip_prot_difficulty_reduction", redPen, kColComponent, kColComponent, true);
                hint += kBreak;
                hint += kColLabel;
                hint += g_pStringTable->translate("ui_uip_prot_bleeding").c_str();
                hint += " ";
                hint += kColTitle;
                hint += g_pStringTable->translate("ui_uip_prot_bleeding_possible").c_str();

                item->set_hint_text(hint.c_str());
            }
        }
    }
}

void ui_actor_state_wnd::UpdateRateHints(CActor* actor)
{
	auto& cv = actor->conditions().change_v();

	// UpdateConditionTime uzywa w SP zegara swiata, a ten bazuje na czasie Device.
	// Przeliczamy wskazniki na sekunde rzeczywistej, niepauzowanej gry.
	// Odczyt biezacego mnoznika obsluguje tez jego zmiane podczas rozgrywki;
	// normal_time_factor jest wartoscia odniesienia, nie aktualnym mnoznikiem.
	// W MP UpdateConditionTime korzysta bezposrednio z zegara serwera.
	const float real_time_factor = IsGameTypeSingle()
		? Level().GetGameTimeFactor() * Device.time_factor() : 1.0f;

	// Dopisuje linie "podpis liczba jednostka". unit_key = nullptr -> bez jednostki.
	auto add_line = [real_time_factor](xr_string& hint, LPCSTR label_key, float value, int decimals, LPCSTR unit_key)
	{
		string32 num;
		FormatRate(num, value * real_time_factor, decimals);

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

	// Kondycja: koszt sprintu (%/s realnej sekundy) i podskoku (% za skok),
	// czerwone przy przeciazeniu. Wzory w CActorCondition (te same co ruch).
	if (m_state[stt_stamina] != nullptr)
	{
		const bool overloaded = actor->conditions().IsOverloaded();
		LPCSTR costColor = overloaded ? "%c[255,210,80,65]" : kColValue;

		xr_string hint = kColTitle;
		hint += g_pStringTable->translate("ui_uip_tt_stamina").c_str();

		auto cost_line = [&hint, costColor](LPCSTR label, float value, LPCSTR unit)
		{
			string32 num;
			FormatRate(num, value, 1);
			hint += kBreak; hint += kColLabel;
			hint += g_pStringTable->translate(label).c_str();
			hint += " "; hint += costColor; hint += num;
			hint += " "; hint += g_pStringTable->translate(unit).c_str();
		};
		cost_line("ui_uip_cost_sprint",
			actor->conditions().GetSprintPowerCostPerGameSec() * real_time_factor * 100.0f,
			"ui_uip_unit_percent_s");
		cost_line("ui_uip_cost_jump",
			actor->conditions().GetJumpPowerCost() * 100.0f,
			"ui_uip_unit_percent");
		m_state[stt_stamina]->set_hint_text(hint.c_str());
	}

	// Skazenie: ubytek zdrowia to radiation_health_v * poziom skazenia,
	// w ulamku zdrowia na sekunde (EntityCondition.cpp, UpdateRadiation).
	if (m_state[stt_radiation] != nullptr)
	{
		const float radiation = actor->conditions().GetRadiation();
		const float drain = actor->conditions().CanBeHarmed() && !GodMode()
			? cv.m_fV_RadiationHealth * radiation : 0.0f;

		xr_string hint = g_pStringTable->translate("ui_uip_tt_rad").c_str();
		if (ConditionUi::RadiationUnitsEnabled())
		{
			auto add_radiation_line = [&hint](LPCSTR label, float value, bool sign, LPCSTR unit, LPCSTR color)
			{
				string32 number;
				ConditionUi::FormatNumber(number, value, ConditionUi::DecimalSeparator(), sign);
				hint += kBreak;
				hint += "%c[255,176,182,186]";
				hint += g_pStringTable->translate(label).c_str();
				hint += " ";
				hint += color;
				hint += number;
				const shared_str suffix = g_pStringTable->translate(unit);
				if (suffix.c_str()[0] != '%')
					hint += " ";
				hint += suffix.c_str();
			};

			float change = actor->conditions().GetRadiationChangeRate() * ConditionUi::RadiationScale;
			// At a resource boundary there is no outward change to display.
			if ((radiation <= 0.0f && change < 0.0f) || (radiation >= 1.0f && change > 0.0f))
				change = 0.0f;
			LPCSTR change_color = change < 0.0f ? "%c[255,110,190,115]"
				: (change > 0.0f ? "%c[255,210,80,65]" : "%c[255,224,230,234]");
			add_radiation_line("ui_uip_tt_rad_level", radiation * ConditionUi::RadiationScale, false,
				"ui_uip_unit_kbq", "%c[255,224,230,234]");
			add_radiation_line("ui_uip_tt_rad_change", change, true, "ui_uip_unit_kbq_s", change_color);
			add_radiation_line("ui_uip_tt_rad_drain", drain * real_time_factor * 100.0f, false,
				"ui_uip_unit_hp_percent_s", drain > 0.0f ? "%c[255,210,80,65]" : "%c[255,224,230,234]");

			string32 number;
			string64 value_text;
			ConditionUi::FormatNumber(number, radiation * ConditionUi::RadiationScale,
				ConditionUi::DecimalSeparator(), false, 0);
			xr_strconcat(value_text, number, " ", g_pStringTable->translate("ui_uip_unit_kbq").c_str());
			m_state[stt_radiation]->set_value_text(value_text);
		}
		else
			add_line(hint, "ui_uip_tt_rad_drain", drain * 100.0f, 3, "ui_uip_unit_hp_s");
        // Optional description follows the live statistics; old string tables
        // without this key keep their original tooltip layout.
        const shared_str description = g_pStringTable->translate("ui_uip_tt_rad_description");
        if (description.size() && xr_strcmp(description.c_str(), "ui_uip_tt_rad_description") != 0)
        {
            hint += kBreak;
            hint += kColSep;
            hint += g_pStringTable->translate("ui_armor_tt_sep").c_str();
            hint += kBreak;
            hint += kColLabel;
            hint += description.c_str();
        }
		m_state[stt_radiation]->set_hint_text(hint.c_str());
	}

	// Krwawienie: ubytek to BleedingSpeed() * bleeding_v, a rany zablizniaja sie
	// tempem wound_incarnation_v + bleeding_restore_speed z kombinezonu
	// i artefaktow (UpdateHealth -> ChangeBleeding). Stad obie liczby w jednej
	// podpowiedzi - to dwie strony tego samego licznika.
	if (m_state[stt_bleeding] && m_state[stt_bleeding]->m_bleeding)
        UpdateBleedingInfo(actor);
    else if (m_state[stt_bleeding] != nullptr)
	{
		const float drain = actor->conditions().BleedingSpeed() * cv.m_fV_Bleeding;
		const float heal  = cv.m_fV_WoundIncarnation + actor->conditions().GetBoostBleedingRestore();

		xr_string hint = g_pStringTable->translate("ui_uip_tt_bleed").c_str();
		add_line(hint, "ui_uip_tt_bleed_drain", drain * 100.0f, 3, "ui_uip_unit_hp_s");
		add_line(hint, "ui_uip_tt_bleed_heal",  heal * 100.0f,  3, "ui_uip_unit_wound_s");
		m_state[stt_bleeding]->set_hint_text(hint.c_str());
	}

    const auto updateRegeneration = [&](EStateType state, bool health)
    {
        ui_actor_state_item* item = m_state[state];
        if (!item)
            return;
        const auto sources = actor->conditions().GetRegenerationSources(health);
        xr_string hint = g_pStringTable->translate(health ? "ui_uip_tt_reg_health" : "ui_uip_tt_reg_power").c_str();
        const auto line = [&](LPCSTR key, float value, bool always = false, bool showSign = true)
        {
            if (!always && value == 0.0f)
                return;
            string64 formatted;
            ConditionUi::FormatRegenerationRate(formatted, value, showSign);
            hint += "\\n%c[255,170,170,170]";
            hint += g_pStringTable->translate(key).c_str();
            hint += ": %c[255,224,230,234]";
            hint += formatted;
        };
        line(health ? "ui_uip_reg_rate_health" : "ui_uip_reg_rate_power", sources.Total(), true, false);
        hint += "\\n";
        line("ui_uip_reg_natural", sources.natural, true, false);
        line("ui_uip_reg_rest", sources.rest);
        line("ui_uip_reg_artefacts", sources.artefacts);
        line("ui_uip_reg_equipment", sources.equipment);
        line("ui_uip_reg_temporary", sources.temporary);
        item->set_hint_text(hint.c_str());
        if (item->m_regeneration)
            item->set_regeneration(ConditionUi::PercentPerSecond(sources.Total(), real_time_factor),
                health ? ConditionUi::HealthRegenerationMaximum : ConditionUi::PowerRegenerationMaximum);
    };
    if (ConditionUi::RegenerationUnitsEnabled())
    {
        updateRegeneration(stt_thirst, true);
        updateRegeneration(stt_power, false);
    }

}

void ui_actor_state_wnd::update_round_states(EStateType stt_type, float initial, float max_power)
{
	auto state = m_state[stt_type];

    if (stt_type == stt_fire || stt_type == stt_shock || stt_type == stt_acid ||
        stt_type == stt_radia || stt_type == stt_psi)
    {
        const float ratio = Protection::DisplayRatio(initial, max_power);
        state->set_protection_overflow(ratio);
        float fill = ratio;
        clamp(fill, 0.0f, 1.0f);
        state->set_progress(fill);
        state->set_arrow(fill);
        string64 text;
        ConditionUi::FormatProtectionPoints(text, ratio, false);
        state->set_text_str(text);
        return;
    }

	const float progress = floor(initial / max_power * 31) / 31; // number of sticks in progress bar
	const float arrow = initial / max_power; //  = 0..1
	
	if (!state->set_progress(progress) && stt_type != stt_main)
	{
		state->set_arrow(arrow);
		state->set_text(arrow);
	}
}

void ui_actor_state_wnd::UpdateHitZone()
{
	if (m_listMode)
	{
		return;
	}

	CUIHudStatesWnd* wnd = CurrentGameUI()->UIMainIngameWnd->get_hud_states();
	VERIFY( wnd );
	if ( !wnd )
	{
		return;
	}
	wnd->UpdateZones();

	if (m_state[stt_main])
	{
		CActor* actor = Level().CurrentViewEntity() ? Level().CurrentViewEntity()->cast_actor() : nullptr;
		float detectRadZonePower = std::max(actor->conditions().m_fRadiationZonePower, wnd->m_radia_hit * 10);
		m_state[stt_main]->set_arrow(detectRadZonePower);
	}
}

void ui_actor_state_wnd::Draw()
{
	inherited::Draw();
	if (m_hint_wnd)
	{
		m_hint_wnd->Draw();
	}
	if (m_environment_hint_wnd)
	{
		m_environment_hint_wnd->Draw();
	}
}

void ui_actor_state_wnd::Show( bool status )
{
	inherited::Show( status );
	ShowChildren( status );
    if (!status && m_environment_hint_wnd)
        m_environment_hint_wnd->set_visible(false);
}

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

void ui_actor_state_item::init_from_xml( CUIXml& xml, const char* path )
{
	CUIXmlInit::InitWindow( xml, path, 0, this);
    m_regeneration = xml.ReadAttribInt(path, 0, "regeneration", 0) != 0;
    m_bleeding = xml.ReadAttribInt(path, 0, "bleeding", 0) != 0;

	XML_NODE* stored_root = xml.GetLocalRoot();
	XML_NODE* new_root = xml.NavigateToNode( path, 0 );
	xml.SetLocalRoot( new_root );

	const char* hint_text = xml.Read( "hint_text", 0, "no hint" );
	set_hint_text_ST( hint_text );
	
	set_hint_delay( (u32)xml.ReadAttribInt( "hint_text", 0, "delay" ) );

	if ( xml.NavigateToNode( "state_progress", 0 ) )	
	{
		m_progress = UIHelper::CreateProgressBar( xml, "state_progress", this );
		m_progress->IsExpressionSystem =
			xml.ReadAttrib(path, 0, "expression", nullptr) != nullptr
			|| xml.ReadAttrib("state_progress", 0, "expression", nullptr) != nullptr;
	}
    if (xml.NavigateToNode("source_progress"))
    {
        m_environmental_exposure = UIHelper::CreateProgressBar(xml, "source_progress", this);
        m_environmental_exposure->Show(false);
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
    if (xml.NavigateToNode("overflow"))
    {
        m_overflow = UIHelper::CreateStatic(xml, "overflow", this);
        m_overflow->Show(false);
    }
    if (xml.NavigateToNode("overflow_fill"))
    {
        m_overflow_fill = UIHelper::CreateStatic(xml, "overflow_fill", this);
        m_overflow_fill->Show(false);
    }
	if (xml.NavigateToNode("value"))
		m_value = UIHelper::CreateStatic(xml, "value", this);
	set_arrow( 0.0f );
	xml.SetLocalRoot( stored_root );
}


bool ui_actor_state_item::set_text( float value )
{
	if (!m_static)
	{
		return false;
	}

	int v = (int)( value * m_magnitude + 0.49f );
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

void ui_actor_state_item::set_value_text(LPCSTR text)
{
	if (m_value)
		m_value->TextItemControl()->SetText(text);
}

void ui_actor_state_item::set_regeneration(float percentPerSecond, float maximum)
{
    string32 number;
    // Positive panel values need no prefix; negative values retain their minus.
    ConditionUi::FormatNumber(number, percentPerSecond, ConditionUi::DecimalSeparator(), false);
    set_value_text(number);
    if (m_progress)
        m_progress->SetProgressPosImmediate(percentPerSecond / maximum);
    const bool overflow = percentPerSecond > maximum;
    if (m_overflow)
        m_overflow->Show(overflow);
    if (m_overflow_fill)
        m_overflow_fill->Show(overflow);
}

void ui_actor_state_item::set_bleeding(float intensity)
{
    string32 number;
    // Ceil the scaled float, including tiny positive bleeding. Never cap the number.
    xr_sprintf(number, sizeof(number), "%.0f", std::ceil(intensity));
    set_value_text(number);
    if (m_progress)
        m_progress->SetProgressPosImmediate(intensity / ConditionUi::BleedingMaximum);
    const bool overflow = intensity > ConditionUi::BleedingMaximum;
    if (m_overflow)
        m_overflow->Show(overflow);
    if (m_overflow_fill)
        m_overflow_fill->Show(overflow);
}

// Use the unclamped ratio: 100 points itself is not overflow.
void ui_actor_state_item::set_protection_overflow(float ratio)
{
    const bool overflow = ratio > 1.0f;
    if (m_overflow)
        m_overflow->Show(overflow);
    if (m_overflow_fill)
        m_overflow_fill->Show(overflow);
}

void ui_actor_state_item::set_environmental_exposure(ALife::EHitType type, float sourceRatio, float protectionRatio, float opacity)
{
    // Both layers compare current values without a lagging equipment animation.
    if (m_progress)
        m_progress->SetProgressPosImmediate(protectionRatio);
    if (!m_environmental_exposure)
        return;
    const bool visible = sourceRatio > 0.0f && opacity > 0.0f;
    m_environmental_exposure->Show(visible);
    if (!visible)
        return;
    const auto color = Protection::GetSourceColor(type);
    clamp(opacity, 0.0f, 1.0f);
    const u32 alpha = u32(255.0f * opacity);
    m_environmental_exposure->m_UIProgressItem.SetTextureColor(color_rgba(color.r, color.g, color.b, alpha));
    m_environmental_exposure->SetProgressPosImmediate(sourceRatio);
}
