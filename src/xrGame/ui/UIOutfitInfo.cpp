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
#include "../ArmorDurability.h"

namespace
{
	float GetOutfitInfoArtefactProtection(CActor& Actor, ALife::EHitType HitType)
	{
		const CEngineExternal& External = EngineExternal();
		if (External.GetBalanceMode() == EEngineExternalBalance::Auto &&
			External.GetEffectiveBalanceMode() == EEngineExternalBalance::CallOfPripyat)
		{
			return 1.0f - Actor.HitArtefactsOnBeltLegacy(1.0f, HitType);
		}
		return Actor.GetProtection_ArtefactsOnBelt(HitType);
	}

	// Typy z kluczem *_immunity w immunities_sect. LightBurn jest kopia burn,
	// a PhysicStrike nie ma klucza, wiec oba sa pominiete.
	constexpr ALife::EHitType WearTypes[] =
	{
		ALife::eHitTypeFireWound, ALife::eHitTypeWound, ALife::eHitTypeStrike, ALife::eHitTypeExplosion,
		ALife::eHitTypeBurn, ALife::eHitTypeShock, ALife::eHitTypeChemicalBurn, ALife::eHitTypeRadiation,
		ALife::eHitTypeTelepatic,
	};
	constexpr unsigned WearTypeCount = sizeof(WearTypes) / sizeof(WearTypes[0]);

	// Liczba pokazywana przy wierszu (punkty i procenty to setne wartosci).
	int Shown(float ratio)
	{
		return iFloor(ratio * 100.0f + 0.5f);
	}

	// Podpisy wierszy szczegolow: odpornosc przedmiotu na zuzycie od typu trafienia.
	const char* WearName(ALife::EHitType type)
	{
		switch (type)
		{
		case ALife::eHitTypeFireWound: return "ui_uip_det_wear_fire_wound";
		case ALife::eHitTypeWound: return "ui_uip_det_wear_wound";
		case ALife::eHitTypeStrike: return "ui_uip_det_wear_strike";
		case ALife::eHitTypeExplosion: return "ui_uip_det_wear_explosion";
		case ALife::eHitTypeBurn: return "ui_uip_det_wear_burn";
		case ALife::eHitTypeShock: return "ui_uip_det_wear_shock";
		case ALife::eHitTypeChemicalBurn: return "ui_uip_det_wear_chemical_burn";
		case ALife::eHitTypeRadiation: return "ui_uip_det_wear_radiation";
		default: return "ui_uip_det_wear_telepatic";
		}
	}

	// Najwyzszy prog przebicia grupy kosci (Protection::ArmorGroups) tego przedmiotu
	// przy podanym stanie; -1 = przedmiot nie kryje zadnej kosci grupy.
	float GroupArmor(IKinematics* ikv, CArmorBase& Item, const Protection::ArmorGroup& Group, float Condition)
	{
		float best = -1.0f;
		for (u32 i = 0; Group.bones[i] != nullptr; ++i)
		{
			const u16 bone = ikv->LL_BoneID(Group.bones[i]);
			if (bone == BI_NONE)
				continue;
			const float armor = Item.GetBoneArmor(bone);
			if (armor >= 0.0f)
				best = std::max(best, armor * Condition);
		}
		return best;
	}

	// Trwalosc przedmiotu: srednia punktow typow, ktore go zuzywaja (z ulepszeniami).
	bool DurabilityMean(CArmorBase& Item, float& Mean)
	{
		float Wear[WearTypeCount];
		for (unsigned i = 0; i < WearTypeCount; ++i)
			Wear[i] = Item.GetHitImmunity(WearTypes[i]);
		return ArmorDurability::MeanPoints(Wear, WearTypeCount, ArmorDurability::Factor(Item.DurabilityPoints), Mean);
	}
}

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

	// Kolumna roznicy od porownywanego przedmiotu (lewy Alt), jak w dymku broni.
	if (xml_doc.NavigateToNode("delta"))
		m_delta = UIHelper::CreateStatic(xml_doc, "delta", this);

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

void CUIOutfitImmunity::SetDelta(int steps)
{
	if (!m_delta)
		return;
	if (steps == 0)
	{
		m_delta->SetText("");
		return;
	}
	string32 text;
	xr_sprintf(text, "%+d", steps);
	m_delta->SetText(text);
	m_delta->SetTextColor(steps > 0 ? m_delta_better : m_delta_worse);
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

// Stan przedmiotu w dymku: z Ctrl jak nowy (1), inaczej faktyczny.
float CUIOutfitInfo::Condition(CArmorBase& item) const
{
	return m_as_new ? 1.0f : item.GetCondition();
}

float CUIOutfitInfo::ItemProtection(CArmorBase& item, ALife::EHitType type) const
{
	return item.GetBaseHitTypeProtection(type) * Condition(item);
}

// "Ochrona mechaniczna" przedmiotu: srednia trzech progow ze stanem.
float CUIOutfitInfo::Mechanical(CArmorBase& item) const
{
	return Protection::MechanicalMean(ItemProtection(item, ALife::eHitTypeWound),
		ItemProtection(item, ALife::eHitTypeStrike), ItemProtection(item, ALife::eHitTypeExplosion));
}

// Najwyzszy prog przebicia krytych kosci ze stanem (GetMaxBoneArmor bez stalego stanu).
float CUIOutfitInfo::MaxBoneArmor(CArmorBase& item) const
{
	const SBoneProtections* bones = item.GetBoneProtections();
	float best = -1.0f;
	if (bones)
	{
		for (SBoneProtections::storage_type::const_iterator it = bones->m_bones_koeff.begin(); it != bones->m_bones_koeff.end(); ++it)
			best = std::max(best, it->second.armor);
	}
	return best < 0.0f ? -1.0f : best * Condition(item);
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
	xr_delete( m_durability );
	xr_delete( m_compare );
	xr_delete( m_details );
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

	// Trwalosc w punktach 2-100: pasek pelny przy 100 (bez przepelnienia). Przed
	// absorpcja i komfortem: bez listy (uklad staly) ukryty komfort helmu
	// zostaje na koncu, zamiast zostawiac luke nad trwaloscia.
	m_durability = CreateItem(xml_doc, "durability", 1.0f, "", "ui_uip_durability");
	if (m_durability && !m_listWnd)
	{
		AttachChild(m_durability);
		m_durability->SetWndPos(pos);
		pos.y += m_durability->GetWndSize().y + RowSpacing;
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
	// Porownanie (lewy Alt): linia nad wierszami i kolory roznic, jak w dymku broni.
	if (xml_doc.NavigateToNode("compare", 0))
	{
		m_compare = UIHelper::CreateStatic(xml_doc, "compare", nullptr);
		m_compare->SetAutoDelete(false);
	}
	const u32 DeltaBetter = CUIXmlInit::GetColor(xml_doc, "delta_better", 0, color_rgba(107, 207, 119, 255));
	const u32 DeltaWorse = CUIXmlInit::GetColor(xml_doc, "delta_worse", 0, color_rgba(196, 18, 18, 255));
	for (u32 i = 0; i < max_count; ++i)
		if (m_items[i])
			m_items[i]->SetDeltaColors(DeltaBetter, DeltaWorse);
	for (CUIOutfitImmunity* extra : { m_impact_absorption, m_stamina_impact, m_durability })
		if (extra)
			extra->SetDeltaColors(DeltaBetter, DeltaWorse);
	// Bloki szczegolow pod parametrami stroju (dodawane do dymka osobno).
	if (xml_doc.NavigateToNode("details", 0))
	{
		m_details = new CUIOutfitDetails();
		m_details->SetAutoDelete(false);
		m_details->InitFromXml(xml_doc, DeltaBetter, DeltaWorse);
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
				float _val_af = GetOutfitInfoArtefactProtection(*Actor(), (ALife::EHitType)i);

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
	// Bez lewego Alt albo wobec samego siebie: tylko wlasne wartosci (ui-armor-tooltip-keys).
	if ( !m_compare_key || slot_outfit == cur_outfit )
		slot_outfit = nullptr;
	const bool compared = slot_outfit != nullptr;
	for ( u32 i = 0; i < max_count; ++i )
	{	
		if ( !m_items[i] || i == ALife::eHitTypeFireWound )
		{
			continue;
		}
		
		ALife::EHitType hit_type = (ALife::EHitType)i;
		if ( hit_type == ALife::eHitTypeWound && !m_items[i]->GetLegacyMode() )
		{
			const float mech = Mechanical( *cur_outfit );
			const float slot_mech = slot_outfit ? Mechanical( *slot_outfit ) : mech;
			m_items[i]->SetPointsValue( mech, slot_mech, Protection::MechanicalBarMax );
			m_items[i]->SetDelta( compared ? Shown( mech ) - Shown( slot_mech ) : 0 );
			ShowInList( m_items[i], true );
			continue;
		}
		float max_power = actor->conditions().GetZoneMaxPower( hit_type );

		float cur = EquipmentProtectionValue( ItemProtection( *cur_outfit, hit_type ), hit_type, max_power );
		float slot = cur;
		
		if ( slot_outfit )
		{
			slot = EquipmentProtectionValue( ItemProtection( *slot_outfit, hit_type ), hit_type, max_power );
		}

		float _val_af = GetOutfitInfoArtefactProtection(*Actor(), hit_type);

		m_items[i]->SetAfValue(_val_af);
		m_items[i]->SetProgressValue( cur, slot );
		m_items[i]->SetDelta( compared ? Shown( cur ) - Shown( slot ) : 0 );

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

		float cur = cur_outfit->GetBoneArmor( spine_bone )*Condition( *cur_outfit );
		float slot = cur;
		if(slot_outfit)
		{
			spine_bone = ikv->LL_BoneID( "bip01_spine" );
			slot = slot_outfit->GetBoneArmor( spine_bone )*Condition( *slot_outfit ); 
		}
		// Roznica liczb panelu "Pancerz" (kosc bez oslony = 0).
		const int armor_delta = compared ? Protection::ArmorPoints( std::max( 0.0f, cur ) ) - Protection::ArmorPoints( std::max( 0.0f, slot ) ) : 0;
		float max_power = actor->conditions().GetMaxFireWoundProtection();
		cur /= max_power;
		slot /= max_power;

		float _val_af = GetOutfitInfoArtefactProtection(*Actor(), ALife::eHitTypeFireWound);

		m_items[ALife::eHitTypeFireWound]->SetAfValue(_val_af);
		m_items[ALife::eHitTypeFireWound]->SetProgressValue( cur, slot );

		// Zamiast liczby: nazwa klasy pancerza z najwyzszego progu przebicia
		// tego elementu (kombinezonu), ta sama klasyfikacja co panel postaci.
		SetArmorClassText( m_items[ALife::eHitTypeFireWound],
			Protection::ArmorClassName( MaxBoneArmor( *cur_outfit ) ) );
		m_items[ALife::eHitTypeFireWound]->SetDelta( armor_delta );

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
		m_impact_absorption->SetDelta( compared ? Shown( cur ) - Shown( slot ) : 0 );
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
		m_stamina_impact->SetDelta( compared ? Shown( cur ) - Shown( slot ) : 0 );
		ShowInList( m_stamina_impact, true );
	}
	UpdateDurability( cur_outfit, slot_outfit );
	UpdateDetails( *cur_outfit, slot_outfit );
}

// Bloki szczegolow (ui-armor-tooltip-keys r2): pancerz stref (grupy kosci panelu
// postaci, tylko ten przedmiot, strefy bez oslony pominiete), trzy progi
// mechaniczne i odpornosc na zuzycie typow, ktore zuzywaja przedmiot. Roznice
// z pokazanych liczb przedmiotu w slocie (tylko z lewym Alt; slot jest wtedy != 0).
void CUIOutfitInfo::UpdateDetails(CArmorBase& cur, CArmorBase* slot)
{
	if (!m_details)
		return;
	m_details->Clear();
	m_details->Line();
	string128 value;

	CActor* actor = Level().CurrentViewEntity() ? Level().CurrentViewEntity()->cast_actor() : nullptr;
	IKinematics* ikv = actor && actor->Visual() ? PKinematics(actor->Visual()) : nullptr;
	if (ikv)
	{
		bool titled = false;
		for (u32 g = 0; g < Protection::ArmorGroupCount; ++g)
		{
			const Protection::ArmorGroup& group = Protection::ArmorGroups[g];
			const float best = GroupArmor(ikv, cur, group, Condition(cur));
			if (best < 0.0f)
				continue;
			if (!titled)
			{
				m_details->Header("ui_uip_det_armor");
				titled = true;
			}
			const char* cls = Protection::ArmorClassName(best);
			if (cls)
				xr_sprintf(value, "%d  %s %s", Protection::ArmorPoints(best), g_pStringTable->translate("ui_uip_armor_class_prefix").c_str(), cls);
			else
				xr_sprintf(value, "%d", Protection::ArmorPoints(best));
			const int delta = slot ? Protection::ArmorPoints(best)
				- Protection::ArmorPoints(std::max(0.0f, GroupArmor(ikv, *slot, group, Condition(*slot)))) : 0;
			m_details->Row(g_pStringTable->translate(group.label_key).c_str(), value, delta);
		}
	}

	m_details->Header("ui_uip_det_mechanical");
	static const std::pair<ALife::EHitType, const char*> Mechanical[] =
	{
		{ ALife::eHitTypeWound, "ui_uip_det_mech_wound" },
		{ ALife::eHitTypeStrike, "ui_uip_det_mech_strike" },
		{ ALife::eHitTypeExplosion, "ui_uip_det_mech_explosion" },
	};
	for (const auto& [type, key] : Mechanical)
	{
		const int points = Shown(ItemProtection(cur, type));
		xr_sprintf(value, "%d", points);
		m_details->Row(g_pStringTable->translate(key).c_str(), value, slot ? points - Shown(ItemProtection(*slot, type)) : 0);
	}

	const float factor = ArmorDurability::Factor(cur.DurabilityPoints);
	const float slot_factor = slot ? ArmorDurability::Factor(slot->DurabilityPoints) : 1.0f;
	bool titled = false;
	for (ALife::EHitType type : WearTypes)
	{
		const float wear = cur.GetHitImmunity(type);
		if (wear <= 0.0f)
			continue;
		if (!titled)
		{
			m_details->Header("ui_uip_det_durability");
			titled = true;
		}
		const int points = iFloor(ArmorDurability::Points(wear * factor) + 0.5f);
		int delta = 0;
		const float slot_wear = slot ? slot->GetHitImmunity(type) : 0.0f;
		if (slot_wear > 0.0f)
			delta = points - iFloor(ArmorDurability::Points(slot_wear * slot_factor) + 0.5f);
		xr_sprintf(value, "%d", points);
		m_details->Row(g_pStringTable->translate(WearName(type)).c_str(), value, delta);
	}
	m_details->Finish();
}

// Wiersz "Trwalosc": srednia punktow (ArmorDurability.h), porownanie z przedmiotem
// w slocie. Nic nie zuzywa przedmiotu albo wiersz w trybie tekstowym: bez wiersza.
void CUIOutfitInfo::UpdateDurability(CArmorBase* cur, CArmorBase* slot)
{
	if (!m_durability)
		return;
	float cur_points = 0.0f;
	const bool shown = !m_durability->GetLegacyMode() && cur && DurabilityMean(*cur, cur_points);
	if (shown)
	{
		float slot_points = cur_points;
		if (slot && !DurabilityMean(*slot, slot_points))
			slot_points = 0.0f;
		m_durability->SetPointsValue(cur_points / 100.0f, slot_points / 100.0f, 1.0f);
		m_durability->SetDelta(slot ? Shown(cur_points / 100.0f) - Shown(slot_points / 100.0f) : 0);
	}
	m_durability->Show(shown);
	ShowInList(m_durability, shown);
}


void CUIOutfitInfo::UpdateInfo(CHelmet* cur_helmet, CHelmet* slot_helmet)
{
	CActor* actor = Level().CurrentViewEntity()->cast_actor();
	if ( !actor || !cur_helmet )
	{
		return;
	}
	if ( !m_compare_key || slot_helmet == cur_helmet )
		slot_helmet = nullptr;
	const bool compared = slot_helmet != nullptr;

	for ( u32 i = 0; i < max_count; ++i )
	{
		if ( i == ALife::eHitTypeFireWound || !m_items[i] )
		{
			continue;
		}
		
		ALife::EHitType hit_type = (ALife::EHitType)i;
		if ( hit_type == ALife::eHitTypeWound && !m_items[i]->GetLegacyMode() )
		{
			const float mech = Mechanical( *cur_helmet );
			const float slot_mech = slot_helmet ? Mechanical( *slot_helmet ) : mech;
			m_items[i]->SetPointsValue( mech, slot_mech, Protection::MechanicalBarMax );
			m_items[i]->SetDelta( compared ? Shown( mech ) - Shown( slot_mech ) : 0 );
			ShowInList( m_items[i], true );
			continue;
		}
		float max_power = actor->conditions().GetZoneMaxPower( hit_type );

		float cur = EquipmentProtectionValue( ItemProtection( *cur_helmet, hit_type ), hit_type, max_power );
		float slot = cur;
		
		if ( slot_helmet )
		{
			slot = EquipmentProtectionValue( ItemProtection( *slot_helmet, hit_type ), hit_type, max_power );
		}
		m_items[i]->SetProgressValue( cur, slot );
		m_items[i]->SetDelta( compared ? Shown( cur ) - Shown( slot ) : 0 );
	}

	if ( m_items[ALife::eHitTypeFireWound] )
	{
		IKinematics* ikv = PKinematics(actor->Visual());
		VERIFY( ikv );
		u16 spine_bone = ikv->LL_BoneID( "bip01_head" );

		float cur = cur_helmet->GetBoneArmor( spine_bone )*Condition( *cur_helmet );
		float slot = (slot_helmet)? slot_helmet->GetBoneArmor( spine_bone )*Condition( *slot_helmet ) : cur;
		
		m_items[ALife::eHitTypeFireWound]->SetProgressValue( cur, slot );
		m_items[ALife::eHitTypeFireWound]->SetDelta( compared ? Protection::ArmorPoints( std::max( 0.0f, cur ) ) - Protection::ArmorPoints( std::max( 0.0f, slot ) ) : 0 );

		// Nazwa klasy pancerza z najwyzszego progu przebicia helmu (maski).
		SetArmorClassText( m_items[ALife::eHitTypeFireWound],
			Protection::ArmorClassName( MaxBoneArmor( *cur_helmet ) ) );
	}

	if ( m_impact_absorption )
	{
		float cur  = 1.0f - cur_helmet->GetHitFractionActor();
		float slot = slot_helmet ? 1.0f - slot_helmet->GetHitFractionActor() : cur;
		clamp( cur, 0.0f, 1.0f );
		clamp( slot, 0.0f, 1.0f );
		m_impact_absorption->SetProgressValue( cur, slot );
		m_impact_absorption->SetDelta( compared ? Shown( cur ) - Shown( slot ) : 0 );
		ShowInList( m_impact_absorption, true );
	}

	// Komfort ruchu nie dotyczy helmu - jego power_loss nie wplywa na kondycje.
	if ( m_stamina_impact )
	{
		m_stamina_impact->Show( false );
		ShowInList( m_stamina_impact, false );
	}
	UpdateDurability( cur_helmet, slot_helmet );
	UpdateDetails( *cur_helmet, slot_helmet );
}

CUIStatic* CUIOutfitInfo::SetCompareLine(const char* name)
{
	if (!m_compare)
		return nullptr;
	string256 text;
	xr_strconcat(text, g_pStringTable->translate("ui_uip_wpn_compare").c_str(), ": ", name);
	m_compare->SetText(text);
	return m_compare;
}

CUIWindow* CUIOutfitInfo::Details() const
{
	return m_details;
}

// ===========================================================================================

CUIOutfitDetails::~CUIOutfitDetails() = default;

// Szablony z <details> w <outfit_info>: <line> (kreska), <header> (naglowek bloku)
// i <row> z <caption>, <value>, <delta>. Wszystkie wiersze tworzone raz.
void CUIOutfitDetails::InitFromXml(CUIXml& xml, u32 better, u32 worse)
{
	CUIXmlInit::InitWindow(xml, "details", 0, this);
	m_better = better;
	m_worse = worse;
	m_line = UIHelper::CreateStatic(xml, "details:line", this, false);
	for (CUIStatic*& header : m_headers)
		header = UIHelper::CreateStatic(xml, "details:header", this, false);
	if (!xml.NavigateToNode("details:row", 0))
		return;
	for (SRow& row : m_rows)
	{
		row.wnd = new CUIWindow();
		row.wnd->SetAutoDelete(true);
		AttachChild(row.wnd);
		CUIXmlInit::InitWindow(xml, "details:row", 0, row.wnd);
		row.caption = UIHelper::CreateStatic(xml, "details:row:caption", row.wnd, false);
		row.value = UIHelper::CreateStatic(xml, "details:row:value", row.wnd, false);
		row.delta = UIHelper::CreateStatic(xml, "details:row:delta", row.wnd, false);
	}
	Clear();
}

void CUIOutfitDetails::Clear()
{
	if (m_line)
		m_line->Show(false);
	for (CUIStatic* header : m_headers)
		if (header)
			header->Show(false);
	for (SRow& row : m_rows)
		if (row.wnd)
			row.wnd->Show(false);
	m_used_headers = 0;
	m_used_rows = 0;
	m_height = 0.0f;
}

void CUIOutfitDetails::Place(CUIWindow* wnd)
{
	wnd->SetWndPos(Fvector2().set(wnd->GetWndPos().x, m_height));
	wnd->Show(true);
	m_height += wnd->GetWndSize().y;
}

void CUIOutfitDetails::Line()
{
	if (m_line)
		Place(m_line);
}

void CUIOutfitDetails::Header(const char* key)
{
	if (m_used_headers >= sizeof(m_headers) / sizeof(m_headers[0]) || !m_headers[m_used_headers])
		return;
	CUIStatic* header = m_headers[m_used_headers++];
	header->SetText(g_pStringTable->translate(key).c_str());
	Place(header);
}

void CUIOutfitDetails::Row(const char* caption, const char* value, int delta)
{
	if (m_used_rows >= sizeof(m_rows) / sizeof(m_rows[0]) || !m_rows[m_used_rows].wnd)
		return;
	SRow& row = m_rows[m_used_rows++];
	if (row.caption)
		row.caption->SetText(caption);
	if (row.value)
		row.value->SetText(value);
	if (row.delta)
	{
		string32 text = "";
		if (delta != 0)
		{
			xr_sprintf(text, "%+d", delta);
			row.delta->SetTextColor(delta > 0 ? m_better : m_worse);
		}
		row.delta->SetText(text);
	}
	Place(row.wnd);
}

void CUIOutfitDetails::Finish()
{
	SetHeight(m_height);
}
