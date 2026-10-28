#include "StdAfx.h"
#include "UIOutfitInfo.h"
#include "UIConditionFormat.h"
#include "../ProtectionValues.h"
#include "../../xrUI/UIXmlInit.h"
#include "../../xrUI/Widgets/UIStatic.h"
#include "../../xrUI/Widgets/UIDoubleProgressBar.h"
#include "UIHelperGame.h"
#include "../../xrUI/Widgets/UIScrollView.h"
#include "../CustomOutfit.h"
#include "../ActorHelmet.h"
#include "../Actor.h"
#include "../ActorCondition.h"
#include "../player_hud.h"
#include "../../xrEngine/string_table.h"
#include "../BoneProtections.h"

constexpr std::tuple<ALife::EHitType, const char*, const char*, float, const char*> outfit_immunity[] =
{
	{ ALife::eHitTypeBurn,			"burn_immunity",            "ui_inv_outfit_burn_protection",            100.0f,      "%" },
	{ ALife::eHitTypeShock,			"shock_immunity",           "ui_inv_outfit_shock_protection",           100.0f,      "%" },
	{ ALife::eHitTypeChemicalBurn,	"chemical_burn_immunity",   "ui_inv_outfit_chemical_burn_protection",   100.0f,      "%" },
	{ ALife::eHitTypeRadiation,		"radiation_immunity",       "ui_inv_outfit_radiation_protection",       100.0f,		 "%" },
	{ ALife::eHitTypeTelepatic,		"telepatic_immunity",       "ui_inv_outfit_telepatic_protection",       100.0f,      "%" },
	{ ALife::eHitTypeWound,			"wound_immunity",           "ui_inv_outfit_wound_protection",           100.0f,      "%" },
	{ ALife::eHitTypeFireWound,		"fire_wound_immunity",      "ui_inv_outfit_fire_wound_protection",      100.0f,      "%" },
	{ ALife::eHitTypeStrike,		"strike_immunity",          "ui_inv_outfit_strike_protection",          100.0f,      "%" },
	{ ALife::eHitTypeExplosion,		"explosion_immunity",       "ui_inv_outfit_explosion_protection",       100.0f,      "%" },
};

CUIOutfitImmunity::CUIOutfitImmunity()
{
	m_unit_str._set("");
	m_value = nullptr;
	m_magnitude = 1.0f;
	m_name = nullptr;
	m_progress = nullptr;
	m_text_legacy = nullptr;
}

CUIOutfitImmunity::~CUIOutfitImmunity()
{
}

const char* outfit_info = "outfit_info";
CUIOutfitImmunity::InitResult CUIOutfitImmunity::Init( CUIXml& xml_doc, const char* section )
{
	if(!CUIXmlInit::InitStatic(xml_doc, section, 0, this, false))
		return InitPlain(xml_doc, section);

	m_name = UIHelper::CreateStatic(xml_doc, section, this);
	m_name->TextureOff();
	
	XML_NODE* base_node = xml_doc.GetLocalRoot();
	xml_doc.SetLocalRoot(xml_doc.NavigateToNode(section));


	if (xml_doc.NavigateToNode("progress_immunity"))
	{
		m_progress = new CUIDoubleProgressBar();
		AttachChild(m_progress);
		m_progress->InitFromXml(xml_doc, "progress_immunity");
	}

	// Opcjonalny trojkat przepelnienia za paskiem, jak w panelu postaci.
	if (xml_doc.NavigateToNode("overflow"))
	{
		m_overflow = UIHelper::CreateStatic(xml_doc, "overflow", this);
		m_overflow->Show(false);
	}
	if (xml_doc.NavigateToNode("overflow_fill"))
	{
		m_overflow_fill = UIHelper::CreateStatic(xml_doc, "overflow_fill", this);
		m_overflow_fill->Show(false);
	}
	
	m_value = UIHelper::CreateStatic(xml_doc, "static_value", this);

	m_magnitude = xml_doc.ReadAttribFlt( "static_value", 0, "magnitude", 1.0f);

	const char* unit_str = xml_doc.ReadAttrib("static_value", 0, "unit_str", "");
	m_unit_str._set(g_pStringTable->translate(unit_str));

	xml_doc.SetLocalRoot(base_node);
	return InitResult::Normal;
}

CUIOutfitImmunity::InitResult CUIOutfitImmunity::InitPlain(CUIXml& xml, const char* section)
{
    string256 buf;
    xr_strconcat(buf, "static_", section);
	if (!CUIXmlInit::InitStatic(xml, buf, 0, this, false))
	{
		return InitResult::Failed;
	}
	m_name = new CUIStatic();
	m_name->SetAutoDelete(true);
	AttachChild(m_name);
	m_name->Show(false); // hack

    m_value = new CUIStatic();
    m_value->SetAutoDelete(true);
    AttachChild(m_value);
    m_value->Show(false); // hack

	TextOff();
	m_text_legacy = UIHelper::CreateStatic(xml, buf, this);
	m_text_legacy->TextureOff();
	m_text_legacy->SetWidth(1000.f); // St4lker0k765: hack for complex mode

	m_legacy_mode = true;

    return InitResult::Plain;
}

void CUIOutfitImmunity::SetValueText(const char* text)
{
	if (m_value)
		m_value->SetText(text);
}

// Wpisuje klase pancerza z prefiksem "kl." (np. "kl. 6a"); brak klasy -> "-".
static void SetArmorClassText(CUIOutfitImmunity* row, const char* cls)
{
	if (!row || row->GetLegacyMode())
		return;
	if (cls)
	{
		string64 buf;
		xr_sprintf(buf, "%s %s", g_pStringTable->translate("ui_uip_armor_class_prefix").c_str(), cls);
		row->SetValueText(buf);
	}
	else
		row->SetValueText(g_pStringTable->translate("ui_armor_tt_none").c_str());
}

// Wklad ochrony elementu: punkty wspolnej skali dla stref, ulamek maksimum dla reszty.
static float EquipmentProtectionValue(float protection, ALife::EHitType hit_type, float max_power)
{
	const float contribution = Protection::EquipmentContribution(protection, hit_type);
	return Protection::IsZoneType(hit_type) ? Protection::DisplayRatio(contribution, max_power) : contribution / max_power;
}

void CUIOutfitImmunity::SetPointsValue(float cur, float comp, float bar_max)
{
	float currentFill = cur / bar_max, comparisonFill = comp / bar_max;
	clamp(currentFill, 0.0f, 1.0f);
	clamp(comparisonFill, 0.0f, 1.0f);
	if (m_progress)
		m_progress->SetTwoPos(currentFill * 100.0f, comparisonFill * 100.0f);
	if (m_value)
	{
		string64 text;
		ConditionUi::FormatProtectionPointsPlain(text, cur);
		m_value->SetText(text);
	}
	const bool overflow = cur > bar_max;
	if (m_overflow)
		m_overflow->Show(overflow);
	if (m_overflow_fill)
		m_overflow_fill->Show(overflow);
}

// "Ochrona mechaniczna" przedmiotu: srednia trzech progow ze stanem.
template <class Armour> static float MechanicalProtection(Armour* item)
{
	return Protection::MechanicalMean(item->GetDefHitTypeProtection(ALife::eHitTypeWound),
		item->GetDefHitTypeProtection(ALife::eHitTypeStrike),
		item->GetDefHitTypeProtection(ALife::eHitTypeExplosion));
}

void CUIOutfitImmunity::SetProgressValue(float cur, float comp)
{
	if (m_zone_protection && !m_legacy_mode)
	{
		float currentFill = cur, comparisonFill = comp;
		clamp(currentFill, 0.0f, 1.0f);
		clamp(comparisonFill, 0.0f, 1.0f);
		if (m_progress)
			m_progress->SetTwoPos(currentFill * 100.0f, comparisonFill * 100.0f);
		string64 text;
		ConditionUi::FormatProtectionPointsPlain(text, cur);
		m_value->SetText(text);
		return;
	}

	cur *= m_magnitude;
	comp *= m_magnitude;
	if (m_progress)
		m_progress->SetTwoPos(cur, comp);

	string32 buf;
	xr_sprintf(buf, "%.0f", cur);

	string256 str;
	if (m_unit_str.size())
		xr_strconcat(str, buf, m_unit_str.c_str());
	else
		xr_strconcat(str, buf);

	m_value->SetText(str);

	// hack
	if (m_legacy_mode)
	{
		int sz = xr_sprintf(str, "%s %s %+3.0f%%", m_name->GetText(), (cur > 0.0f) ? "%c[green]" : "%c[red]", cur);
		if (!fis_zero(m_af_value))
		{
			sz += xr_sprintf(str + sz, sizeof(str) - sz, " %s %+3.0f%%", (m_af_value > 0.0f) ? "%c[green]" : "%c[red]", m_af_value*100.f);
		}

		m_text_legacy->SetText(str);
	}
}


void CUIOutfitImmunity::SetDefaultValuesPlain(float magnitude, const shared_str& unit)
{
	m_magnitude = magnitude;
	m_unit_str = unit;
}

void CUIOutfitImmunity::SetCaption(const char* name)
{
	m_name->TextItemControl()->SetText(name);
}

// ===========================================================================================

CUIOutfitInfo::CUIOutfitInfo()
{
	m_Prop_line = nullptr;
	m_listWnd = nullptr;
	for ( u32 i = 0; i < max_count; ++i )
	{
		m_items[i] = nullptr;
	}
}

CUIOutfitInfo::~CUIOutfitInfo()
{
	for ( u32 i = 0; i < max_count; ++i )
	{
		xr_delete( m_items[i] );
	}
	xr_delete( m_impact_absorption );
	xr_delete( m_stamina_impact );
}

void CUIOutfitInfo::ShowInList(CUIOutfitImmunity* item, bool visible)
{
	if (!m_listWnd || !item)
		return;
	if (item->GetParent())
		m_listWnd->RemoveWindow(item);
	if (visible)
		m_listWnd->AddWindow(item, false);
}

void CUIOutfitInfo::InitFromXml( CUIXml& xml_doc )
{
	XML_NODE* stored_root = xml_doc.GetLocalRoot();
	XML_NODE* base_node = xml_doc.NavigateToNode(outfit_info, 0);
	if (!base_node)
	{
		return;
	}
	CUIXmlInit::InitWindow( xml_doc, outfit_info, 0, this );
	const float RowSpacing = xml_doc.ReadAttribFlt(outfit_info, 0, "row_spacing", 0.0f);
	xml_doc.SetLocalRoot(base_node);
	
	string256 buf;

	m_listWnd = UIHelper::CreateScrollView(xml_doc, "scroll_view", this, false);

	m_caption = UIHelper::CreateStatic(xml_doc, "caption", this, false);

	m_Prop_line = UIHelper::CreateStatic(xml_doc, "prop_line", this, false);

	Fvector2 pos;
	if (m_Prop_line)
		pos.set(0.0f, m_Prop_line->GetWndPos().y + m_Prop_line->GetWndSize().y);
	else if (m_caption)
		pos.set(0.0f, m_caption->GetWndSize().y);

	for (auto [id, section, caption, magnitude, unit] : outfit_immunity)
	{
		m_items[id] = CreateItem(xml_doc, section, magnitude, unit, caption);
		if (m_items[id])
			m_items[id]->SetZoneProtection(Protection::IsZoneType(id));
		if (m_items[id] && !m_listWnd)
		{
			AttachChild(m_items[id]);
			m_items[id]->SetWndPos(pos);
			pos.y += m_items[id]->GetWndSize().y + RowSpacing;
		}
	}

	// Dodatkowe wiersze spoza tablicy hit-typow (jesli sa w XML), prezentowane procentowo.
	for (CUIOutfitImmunity** extra : { &m_impact_absorption, &m_stamina_impact })
	{
		const bool impact = extra == &m_impact_absorption;
		*extra = CreateItem(xml_doc, impact ? "impact_absorption" : "stamina_impact", 100.0f, "%",
			impact ? "ui_uip_impact_absorption" : "ui_uip_stamina_impact");
		if (*extra && !m_listWnd)
		{
			AttachChild(*extra);
			(*extra)->SetWndPos(pos);
			pos.y += (*extra)->GetWndSize().y + RowSpacing;
		}
	}
	pos.x = GetWndSize().x;
	SetWndSize( pos );
	xml_doc.SetLocalRoot(stored_root);
}

CUIOutfitImmunity* CUIOutfitInfo::CreateItem(CUIXml& uiXml, const char* section,
    float magnitude, const shared_str& unit,
    shared_str translationId)
{
	CUIOutfitImmunity* item = new CUIOutfitImmunity();

	const CUIOutfitImmunity::InitResult result = item->Init(uiXml, section);
	switch (result)
	{
	case CUIOutfitImmunity::InitResult::Failed:
		xr_delete(item);
		return nullptr;

	case CUIOutfitImmunity::InitResult::Plain:
		item->SetDefaultValuesPlain(magnitude, unit);
		break;
	}

	item->SetCaption(g_pStringTable->translate(translationId).c_str());

	item->SetAutoDelete(false);
	return item;
}

void CUIOutfitInfo::UpdateInfo(CCustomOutfit* cur_outfit, CCustomOutfit* slot_outfit)
{
	CActor* actor = Level().CurrentViewEntity()->cast_actor();
	if ( !actor || !cur_outfit )
	{
		for (u32 i = 0; i < max_count; ++i)
		{
			if (m_items[i] && m_items[i]->GetLegacyMode())
			{
				float _val_af = Actor()->HitArtefactsOnBeltLegacy(1.0f, (ALife::EHitType)i);
				_val_af = 1.0f - _val_af;

				m_items[i]->SetAfValue(_val_af);
				m_items[i]->SetProgressValue(0.0f, 0.0f);

				if (!m_items[i]->GetParent() && !fis_zero(_val_af))
				{
					m_listWnd->AddWindow(m_items[i], false);
				}
				else if (m_items[i]->GetParent() && fis_zero(_val_af))
					m_listWnd->RemoveWindow(m_items[i]);
			}
		}
		return;
	}
	for ( u32 i = 0; i < max_count; ++i )
	{	
		if ( !m_items[i] || i == ALife::eHitTypeFireWound )
		{
			continue;
		}
		
		ALife::EHitType hit_type = (ALife::EHitType)i;
		if ( hit_type == ALife::eHitTypeWound && !m_items[i]->GetLegacyMode() )
		{
			const float mech = MechanicalProtection( cur_outfit );
			m_items[i]->SetPointsValue( mech, slot_outfit ? MechanicalProtection( slot_outfit ) : mech, Protection::MechanicalBarMax );
			ShowInList( m_items[i], true );
			continue;
		}
		float max_power = actor->conditions().GetZoneMaxPower( hit_type );

		float cur = EquipmentProtectionValue( cur_outfit->GetDefHitTypeProtection( hit_type ), hit_type, max_power );
		float slot = cur;
		
		if ( slot_outfit )
		{
			slot = EquipmentProtectionValue( slot_outfit->GetDefHitTypeProtection( hit_type ), hit_type, max_power );
		}

		float _val_af = Actor()->HitArtefactsOnBeltLegacy(1.0f, hit_type);
		_val_af = 1.0f - _val_af;

		m_items[i]->SetAfValue(_val_af);
		m_items[i]->SetProgressValue( cur, slot );

		if (m_listWnd)
		{
			if (m_items[i]->GetParent())
				m_listWnd->RemoveWindow(m_items[i]);

			if (!fis_zero(_val_af) || !fis_zero(cur))
				m_listWnd->AddWindow(m_items[i], false);
		}
	}

	if ( m_items[ALife::eHitTypeFireWound] )
	{
		IKinematics* ikv = PKinematics(actor->Visual());
		VERIFY( ikv );
		u16 spine_bone = ikv->LL_BoneID( "bip01_spine" );

		float cur = cur_outfit->GetBoneArmor( spine_bone )*cur_outfit->GetCondition();
		float slot = cur;
		if(slot_outfit)
		{
			spine_bone = ikv->LL_BoneID( "bip01_spine" );
			slot = slot_outfit->GetBoneArmor( spine_bone )*slot_outfit->GetCondition(); 
		}
		float max_power = actor->conditions().GetMaxFireWoundProtection();
		cur /= max_power;
		slot /= max_power;

		float _val_af = Actor()->HitArtefactsOnBeltLegacy(1.0f, ALife::eHitTypeFireWound);
		_val_af = 1.0f - _val_af;

		m_items[ALife::eHitTypeFireWound]->SetAfValue(_val_af);
		m_items[ALife::eHitTypeFireWound]->SetProgressValue( cur, slot );

		// Zamiast liczby: nazwa klasy pancerza z najwyzszego progu przebicia
		// tego elementu (kombinezonu), ta sama klasyfikacja co panel postaci.
		SetArmorClassText( m_items[ALife::eHitTypeFireWound],
			Protection::ArmorClassName( cur_outfit->GetMaxBoneArmor() ) );

		if (m_listWnd)
		{
			if (m_items[ALife::eHitTypeFireWound]->GetParent())
				m_listWnd->RemoveWindow(m_items[ALife::eHitTypeFireWound]);

			if (!fis_zero(_val_af) || !fis_zero(cur))
				m_listWnd->AddWindow(m_items[ALife::eHitTypeFireWound], false);
		}
	}

	if ( m_impact_absorption )
	{
		float cur  = 1.0f - cur_outfit->GetHitFractionActor();
		float slot = slot_outfit ? 1.0f - slot_outfit->GetHitFractionActor() : cur;
		clamp( cur, 0.0f, 1.0f );
		clamp( slot, 0.0f, 1.0f );
		m_impact_absorption->SetProgressValue( cur, slot );
		ShowInList( m_impact_absorption, true );
	}

	if ( m_stamina_impact )
	{
		// Komfort ruchu: gola postac ma mnoznik 0.5, kombinezon m_fPowerLoss
		// (wyzej = wiekszy drenaz kondycji). 0.5 -> 100%, 1.0 -> 0%.
		float cur  = ( 1.0f - cur_outfit->m_fPowerLoss ) * 2.0f;
		float slot = slot_outfit ? ( 1.0f - slot_outfit->m_fPowerLoss ) * 2.0f : cur;
		clamp( cur, 0.0f, 1.0f );
		clamp( slot, 0.0f, 1.0f );
		m_stamina_impact->Show( true );
		m_stamina_impact->SetProgressValue( cur, slot );
		ShowInList( m_stamina_impact, true );
	}
}


void CUIOutfitInfo::UpdateInfo(CHelmet* cur_helmet, CHelmet* slot_helmet)
{
	CActor* actor = Level().CurrentViewEntity()->cast_actor();
	if ( !actor || !cur_helmet )
	{
		return;
	}

	for ( u32 i = 0; i < max_count; ++i )
	{
		if ( i == ALife::eHitTypeFireWound || !m_items[i] )
		{
			continue;
		}
		
		ALife::EHitType hit_type = (ALife::EHitType)i;
		if ( hit_type == ALife::eHitTypeWound && !m_items[i]->GetLegacyMode() )
		{
			const float mech = MechanicalProtection( cur_helmet );
			m_items[i]->SetPointsValue( mech, slot_helmet ? MechanicalProtection( slot_helmet ) : mech, Protection::MechanicalBarMax );
			ShowInList( m_items[i], true );
			continue;
		}
		float max_power = actor->conditions().GetZoneMaxPower( hit_type );

		float cur = EquipmentProtectionValue( cur_helmet->GetDefHitTypeProtection( hit_type ), hit_type, max_power );
		float slot = cur;
		
		if ( slot_helmet )
		{
			slot = EquipmentProtectionValue( slot_helmet->GetDefHitTypeProtection( hit_type ), hit_type, max_power );
		}
		m_items[i]->SetProgressValue( cur, slot );
	}

	if ( m_items[ALife::eHitTypeFireWound] )
	{
		IKinematics* ikv = PKinematics(actor->Visual());
		VERIFY( ikv );
		u16 spine_bone = ikv->LL_BoneID( "bip01_head" );

		float cur = cur_helmet->GetBoneArmor( spine_bone )*cur_helmet->GetCondition();
		float slot = (slot_helmet)? slot_helmet->GetBoneArmor( spine_bone )*slot_helmet->GetCondition() : cur;
		
		m_items[ALife::eHitTypeFireWound]->SetProgressValue( cur, slot );

		// Nazwa klasy pancerza z najwyzszego progu przebicia helmu (maski).
		SetArmorClassText( m_items[ALife::eHitTypeFireWound],
			Protection::ArmorClassName( cur_helmet->GetMaxBoneArmor() ) );
	}

	if ( m_impact_absorption )
	{
		float cur  = 1.0f - cur_helmet->GetHitFractionActor();
		float slot = slot_helmet ? 1.0f - slot_helmet->GetHitFractionActor() : cur;
		clamp( cur, 0.0f, 1.0f );
		clamp( slot, 0.0f, 1.0f );
		m_impact_absorption->SetProgressValue( cur, slot );
		ShowInList( m_impact_absorption, true );
	}

	// Komfort ruchu nie dotyczy helmu - jego power_loss nie wplywa na kondycje.
	if ( m_stamina_impact )
	{
		m_stamina_impact->Show( false );
		ShowInList( m_stamina_impact, false );
	}
}
