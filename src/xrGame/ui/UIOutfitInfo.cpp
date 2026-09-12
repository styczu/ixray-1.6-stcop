#include "StdAfx.h"
#include "UIOutfitInfo.h"
#include "UIConditionFormat.h"
#include "../ProtectionValues.h"
#include "../../xrUI/UIXmlInit.h"
#include "../../xrUI/Widgets/UIStatic.h"
#include "../../xrUI/Widgets/UIDoubleProgressBar.h"
#include "UIHelperGame.h"

#include "../CustomOutfit.h"
#include "../ActorHelmet.h"
#include "../Actor.h"
#include "../ActorCondition.h"
#include "../player_hud.h"
#include "../../xrEngine/string_table.h"


LPCSTR immunity_names[]=
{
	"burn_immunity",
	"shock_immunity",
	"chemical_burn_immunity",
	"radiation_immunity",
	"telepatic_immunity",
	"wound_immunity",		
	"fire_wound_immunity",
	"strike_immunity",
	"explosion_immunity",
};

LPCSTR immunity_st_names[]=
{
	"ui_inv_outfit_burn_protection",
	"ui_inv_outfit_shock_protection",
	"ui_inv_outfit_chemical_burn_protection",
	"ui_inv_outfit_radiation_protection",
	"ui_inv_outfit_telepatic_protection",
	"ui_inv_outfit_wound_protection",
	"ui_inv_outfit_fire_wound_protection",
	"ui_inv_outfit_strike_protection",
	"ui_inv_outfit_explosion_protection",
};

CUIOutfitImmunity::CUIOutfitImmunity()
{
	AttachChild(&m_name);
	AttachChild(&m_progress);
	m_unit_str._set("");
	m_value = nullptr;
	m_magnitude = 1.0f;
}

CUIOutfitImmunity::~CUIOutfitImmunity()
{
}

bool CUIOutfitImmunity::InitFromXml( CUIXml& xml_doc, LPCSTR base_str, u32 hit_type )
{
    m_zone_protection = Protection::IsZoneType((ALife::EHitType)hit_type);
    return InitRow( xml_doc, base_str, immunity_names[hit_type], immunity_st_names[hit_type] );
}

bool CUIOutfitImmunity::InitFromNode( CUIXml& xml_doc, LPCSTR base_str, LPCSTR node_name, LPCSTR st_name )
{
    m_zone_protection = false; // nowe wiersze prezentujemy procentowo
    return InitRow( xml_doc, base_str, node_name, st_name );
}

bool CUIOutfitImmunity::InitRow( CUIXml& xml_doc, LPCSTR base_str, LPCSTR node_name, LPCSTR st_name )
{
	CUIXmlInit::InitWindow( xml_doc, base_str, 0, this );

	string256 buf;

	xr_strcpy( m_node_name, node_name );

	xr_strconcat(buf, base_str, ":", node_name );
	if (!CUIXmlInit::InitWindow( xml_doc, buf, 0, this, false ))
		return false;

	CUIXmlInit::InitStatic( xml_doc, buf, 0, &m_name );
	m_name.TextItemControl()->SetTextST( st_name );

	xr_strconcat(buf, base_str, ":", node_name, ":progress_immunity" );
	m_progress.InitFromXml( xml_doc, buf );

	xr_strconcat(buf, base_str, ":", node_name, ":static_value" );
	m_value = UIHelper::CreateTextWnd(xml_doc, buf, this);

	m_magnitude = xml_doc.ReadAttribFlt( buf, 0, "magnitude", 1.0f );

	LPCSTR unit_str = xml_doc.ReadAttrib(buf, 0, "unit_str", "");
	m_unit_str._set(g_pStringTable->translate(unit_str));

	// Uklad dwuliniowy stosujemy do KAZDEGO wiersza (nie tylko strefowych):
	// szerokie paski (250 px) nie zmiescilyby sie w linii z etykieta i liczba.
	ApplyTwoLineLayout( xml_doc, base_str );
	return true;
}

void CUIOutfitImmunity::ApplyTwoLineLayout( CUIXml& xml_doc, LPCSTR base_str )
{
    if (!m_value) // wiersz bez wezla static_value - nic nie ukladamy
        return;
    const float right = m_value->GetWndPos().x + m_value->GetWidth();
    m_value->SetWidth(75.0f);
    m_value->SetWndPos(Fvector2().set(right - 75.0f, m_value->GetWndPos().y));
    m_value->SetTextAlignment(CGameFont::alRight);
    const float barY = _max(m_name.GetHeight(), m_value->GetHeight()) + 2.0f;
    string256 buf;
    xr_strconcat(buf, base_str, ":", m_node_name, ":progress_immunity");
    const float childY = xml_doc.ReadAttribFlt(buf, 0, "y", 0.0f);
    const float barHeight = xml_doc.ReadAttribFlt(buf, 0, "height", 9.0f);
    m_progress.SetWndPos(Fvector2().set(0.0f, barY - childY));
    SetHeight(barY + barHeight + 3.0f);
}

void CUIOutfitImmunity::SetProgressValue(float cur, float comp)
{
    if (m_zone_protection)
    {
        float currentFill = cur, comparisonFill = comp;
        clamp(currentFill, 0.0f, 1.0f);
        clamp(comparisonFill, 0.0f, 1.0f);
        m_progress.SetTwoPos(currentFill * 100.0f, comparisonFill * 100.0f);
        string64 text;
        ConditionUi::FormatProtectionPointsPlain(text, cur);
        m_value->SetText(text);
        return;
    }

	cur *= m_magnitude;
	comp *= m_magnitude;
	m_progress.SetTwoPos(cur, comp);

	string32 buf;
	xr_sprintf(buf, "%.0f", cur);

	string256 str;
	if (m_unit_str.size())
		xr_strconcat(str, buf, m_unit_str.c_str());
	else
		xr_strconcat(str, buf);

	m_value->SetText(str);
}

// ===========================================================================================

CUIOutfitInfo::CUIOutfitInfo()
{
	m_Prop_line = nullptr;
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

void CUIOutfitInfo::InitFromXml( CUIXml& xml_doc )
{
	LPCSTR base_str	= "outfit_info";

	CUIXmlInit::InitWindow( xml_doc, base_str, 0, this );
	
	string128 buf;

	xr_strconcat(buf, base_str, ":caption");
	if (xml_doc.NavigateToNode(buf))
	{
		m_caption = UIHelper::CreateStatic(xml_doc, buf, this);
	}

	xr_strconcat(buf, base_str, ":", "prop_line");
	if (xml_doc.NavigateToNode(buf))
	{
		m_Prop_line = UIHelper::CreateStatic(xml_doc, buf, this);
	}

	Fvector2 pos;
	if (m_Prop_line)
		pos.set(0.0f, m_Prop_line->GetWndPos().y + m_Prop_line->GetWndSize().y);
	else if (m_caption)
		pos.set(0.0f, m_caption->GetWndSize().y);

	for ( u32 i = 0; i < max_count; ++i )
	{
		m_items[i] = new CUIOutfitImmunity();
		if (m_items[i]->InitFromXml(xml_doc, base_str, i))
		{
			AttachChild(m_items[i]);
			m_items[i]->SetWndPos(pos);
			pos.y += m_items[i]->GetWndSize().y;
		}
		else
		{
			xr_delete(m_items[i]);
		}
	}

	// Dodatkowe wiersze spoza tablicy hit-typow (jesli sa w XML).
	m_impact_absorption = new CUIOutfitImmunity();
	if (m_impact_absorption->InitFromNode(xml_doc, base_str, "impact_absorption", "ui_uip_impact_absorption"))
	{
		AttachChild(m_impact_absorption);
		m_impact_absorption->SetWndPos(pos);
		pos.y += m_impact_absorption->GetWndSize().y;
	}
	else
	{
		xr_delete(m_impact_absorption);
	}

	m_stamina_impact = new CUIOutfitImmunity();
	if (m_stamina_impact->InitFromNode(xml_doc, base_str, "stamina_impact", "ui_uip_stamina_impact"))
	{
		AttachChild(m_stamina_impact);
		m_stamina_impact->SetWndPos(pos);
		pos.y += m_stamina_impact->GetWndSize().y;
	}
	else
	{
		xr_delete(m_stamina_impact);
	}

	pos.x = GetWndSize().x;
	SetWndSize( pos );
}

void CUIOutfitInfo::UpdateInfo(CCustomOutfit* cur_outfit, CCustomOutfit* slot_outfit)
{
	CActor* actor = Level().CurrentViewEntity()->cast_actor();
	if ( !actor || !cur_outfit )
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
		float max_power = actor->conditions().GetZoneMaxPower( hit_type );

		float cur = Protection::EquipmentContribution(cur_outfit->GetDefHitTypeProtection( hit_type ), hit_type);
		cur = Protection::IsZoneType(hit_type) ? Protection::DisplayRatio(cur, max_power) : cur / max_power;
		float slot = cur;
		
		if ( slot_outfit )
		{
			slot = Protection::EquipmentContribution(slot_outfit->GetDefHitTypeProtection( hit_type ), hit_type);
			slot = Protection::IsZoneType(hit_type) ? Protection::DisplayRatio(slot, max_power) : slot / max_power;
		}
		m_items[i]->SetProgressValue( cur, slot );
	}

	if ( m_items[ALife::eHitTypeFireWound] )
	{
		IKinematics* ikv = PKinematics(actor->Visual());
		VERIFY( ikv );
		u16 spine_bone = ikv->LL_BoneID( "bip01_spine" );

		float cur = cur_outfit->GetBoneArmor( spine_bone )*cur_outfit->GetCondition();
		//if(!cur_outfit->bIsHelmetAvaliable)
		//{
		//	spine_bone = ikv->LL_BoneID("bip01_head");
		//	cur += cur_outfit->GetBoneArmor(spine_bone);
		//}
		float slot = cur;
		if(slot_outfit)
		{
			spine_bone = ikv->LL_BoneID( "bip01_spine" );
			slot = slot_outfit->GetBoneArmor( spine_bone )*slot_outfit->GetCondition(); 
			//if(!slot_outfit->bIsHelmetAvaliable)
			//{
			//	spine_bone = ikv->LL_BoneID("bip01_head");
			//	slot += slot_outfit->GetBoneArmor(spine_bone);
			//}
		}
		float max_power = actor->conditions().GetMaxFireWoundProtection();
		cur /= max_power;
		slot /= max_power;
		m_items[ALife::eHitTypeFireWound]->SetProgressValue( cur, slot );
	}

	if ( m_impact_absorption )
	{
		float cur  = 1.0f - cur_outfit->GetHitFractionActor();
		float slot = slot_outfit ? 1.0f - slot_outfit->GetHitFractionActor() : cur;
		clamp( cur, 0.0f, 1.0f );
		clamp( slot, 0.0f, 1.0f );
		m_impact_absorption->SetProgressValue( cur, slot );
	}

	if ( m_stamina_impact )
	{
		float cur  = 1.0f - cur_outfit->m_fPowerLoss;
		float slot = slot_outfit ? 1.0f - slot_outfit->m_fPowerLoss : cur;
		clamp( cur, 0.0f, 1.0f );
		clamp( slot, 0.0f, 1.0f );
		m_stamina_impact->SetProgressValue( cur, slot );
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
		float max_power = actor->conditions().GetZoneMaxPower( hit_type );

		float cur = Protection::EquipmentContribution(cur_helmet->GetDefHitTypeProtection( hit_type ), hit_type);
		cur = Protection::IsZoneType(hit_type) ? Protection::DisplayRatio(cur, max_power) : cur / max_power;
		float slot = cur;
		
		if ( slot_helmet )
		{
			slot = Protection::EquipmentContribution(slot_helmet->GetDefHitTypeProtection( hit_type ), hit_type);
			slot = Protection::IsZoneType(hit_type) ? Protection::DisplayRatio(slot, max_power) : slot / max_power;
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
	}

	if ( m_impact_absorption )
	{
		float cur  = 1.0f - cur_helmet->GetHitFractionActor();
		float slot = slot_helmet ? 1.0f - slot_helmet->GetHitFractionActor() : cur;
		clamp( cur, 0.0f, 1.0f );
		clamp( slot, 0.0f, 1.0f );
		m_impact_absorption->SetProgressValue( cur, slot );
	}

	if ( m_stamina_impact )
	{
		float cur  = 1.0f - cur_helmet->m_fPowerLoss;
		float slot = slot_helmet ? 1.0f - slot_helmet->m_fPowerLoss : cur;
		clamp( cur, 0.0f, 1.0f );
		clamp( slot, 0.0f, 1.0f );
		m_stamina_impact->SetProgressValue( cur, slot );
	}
}
