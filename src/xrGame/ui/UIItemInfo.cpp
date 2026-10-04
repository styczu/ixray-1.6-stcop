#include "StdAfx.h"
#include "pch_script.h"

#include "UIItemInfo.h"
#include "../../xrUI/Widgets/UIStatic.h"
#include "../../xrUI/UIXmlInit.h"

#include "../../xrUI/Widgets/UIItemStateDisplay.h"
#include "../../xrUI/Widgets/UIScrollView.h"
#include "../../xrUI/Widgets/UIFrameWindow.h"

#include "ai_space.h"
#include "alife_simulator.h"
#include "Inventory.h"
#include "InventoryVolumeSystem.h"
#include "../../xrEngine/string_table.h"
#include "../../xrEngine/xr_input.h"
#include "../inventory_item.h"
#include "UIInventoryUtilities.h"
#include "../PhysicsShellHolder.h"
#include "UIWpnParams.h"
#include "UIWeaponParams.h"
#include "UIKnifeParams.h"
#include "ui_af_params.h"
#include "UIInvUpgradeProperty.h"
#include "UIOutfitInfo.h"
#include "UIBoosterInfo.h"
#include "UIAmmoParams.h"
#include "../Weapon.h"
#include "../CustomOutfit.h"
#include "../ActorHelmet.h"
#include "../eatable_item.h"
#include "UICellItem.h"
#include "UIGrenadeParams.h"
#include "../ImUtils/ImUtils.h"

extern const char* g_inventory_upgrade_xml;

#define INV_GRID_WIDTH2(SCALE_ICON) (40.0f * SCALE_ICON)
#define INV_GRID_HEIGHT2(SCALE_ICON) (40.0f * SCALE_ICON)

CUIItemInfo::CUIItemInfo()
{
	UIItemImageSize.set			(0.0f,0.0f);
	
	UICost						= nullptr;
	UITradeTip					= nullptr;
	UIWeight					= nullptr;
	UIItemImage					= nullptr;
	UIDesc						= nullptr;
	UIConditionWnd				= nullptr;
	UIWpnParams					= nullptr;
	UIWeaponParams				= nullptr;
	UIKnifeParams				= nullptr;
	UIGrenadeParams				= nullptr;
	UIProperties				= nullptr;
	UIUpgradesSide				= nullptr;
	UIUpgradesFrame				= nullptr;
	m_upgrades_side				= false;
	UIOutfitInfo				= nullptr;
	UIBoosterInfo				= nullptr;
	UIAmmoParams				= nullptr;
	UIArtefactParams			= nullptr;
	UIOutfitParams				= nullptr;
	UIBackpackParams			= nullptr;
	UIName						= nullptr;
	UIBackground				= nullptr;
	UICondition					= nullptr;
	UICondProgresBar			= nullptr;
	m_pInvItem					= nullptr;
	m_b_FitToHeight				= false;
	m_complex_desc				= false;
}

CUIItemInfo::~CUIItemInfo()
{
	xr_delete	(UIConditionWnd);
	xr_delete	(UIWpnParams);
	xr_delete	(UIWeaponParams);
	xr_delete	(UIKnifeParams);
	xr_delete	(UIGrenadeParams);
	xr_delete	(UIArtefactParams);
	xr_delete	(UIOutfitParams);
	xr_delete	(UIBackpackParams);
	// The side window sits in its frame; the child goes first.
	xr_delete	(UIUpgradesSide);
	xr_delete	(UIUpgradesFrame);
	xr_delete	(UIProperties);
	xr_delete	(UIOutfitInfo);
	xr_delete	(UIBoosterInfo);
	xr_delete	(UIAmmoParams);
}

bool CUIItemInfo::InitItemInfo(const char* xml_name)
{
	CUIXml						uiXml;
	uiXml.Load					(CONFIG_PATH, UI_PATH, xml_name);

	if (uiXml.GetNodesNum(uiXml.GetRoot(), nullptr) == 0)
		return false;

	CUIXmlInit					xml_init;

	if(uiXml.NavigateToNode("main_frame",0))
	{
		Frect wnd_rect;
		wnd_rect.x1		= uiXml.ReadAttribFlt("main_frame", 0, "x", 0);
		wnd_rect.y1		= uiXml.ReadAttribFlt("main_frame", 0, "y", 0);

		wnd_rect.x2		= uiXml.ReadAttribFlt("main_frame", 0, "width", 0);
		wnd_rect.y2		= uiXml.ReadAttribFlt("main_frame", 0, "height", 0);
		wnd_rect.x2		+= wnd_rect.x1;
		wnd_rect.y2		+= wnd_rect.y1;
		inherited::SetWndRect(wnd_rect);
		
		delay			= uiXml.ReadAttribInt("main_frame", 0, "delay", 500);
	}
	if(uiXml.NavigateToNode("background_frame",0))
	{
		UIBackground				= new CUIFrameWindow();
		UIBackground->SetAutoDelete	(true);
		AttachChild					(UIBackground);
		xml_init.InitFrameWindow	(uiXml, "background_frame", 0,	UIBackground);
	}
	m_complex_desc = false;
	if(uiXml.NavigateToNode("static_name",0))
	{
		UIName						= new CUIStatic();
		AttachChild					(UIName);		
		UIName->SetAutoDelete		(true);
		xml_init.InitStatic		(uiXml, "static_name", 0,	UIName);
		m_complex_desc				= ( uiXml.ReadAttribInt("static_name", 0, "complex_desc", 0) == 1 );
	}
	if(uiXml.NavigateToNode("static_weight",0))
	{
		UIWeight				= new CUIStatic();
		AttachChild				(UIWeight);		
		UIWeight->SetAutoDelete(true);
		xml_init.InitStatic		(uiXml, "static_weight", 0,			UIWeight);
	}

	if(uiXml.NavigateToNode("static_cost",0))
	{
		UICost					= new CUIStatic();
		AttachChild				(UICost);
		UICost->SetAutoDelete	(true);
		xml_init.InitStatic		(uiXml, "static_cost", 0,			UICost);
	}
	
	if(uiXml.NavigateToNode("static_condition",0)) // for SoC
	{
		UICondition					= new CUIStatic();	 
		AttachChild					(UICondition);
		UICondition->SetAutoDelete	(true);
		xml_init.InitStatic			(uiXml, "static_condition", 0,		UICondition);
	}
	
	if(uiXml.NavigateToNode("condition_progress",0))
	{
		UICondProgresBar = new CUIProgressBar();
		AttachChild(UICondProgresBar);
		UICondProgresBar->SetAutoDelete(true);
		xml_init.InitProgressBar(uiXml, "condition_progress", 0, UICondProgresBar);
	}

	if(uiXml.NavigateToNode("static_no_trade",0))
	{
		UITradeTip					= new CUIStatic();
		AttachChild					(UITradeTip);
		UITradeTip->SetAutoDelete	(true);
		xml_init.InitStatic		(uiXml, "static_no_trade", 0,		UITradeTip);
	}

	if(uiXml.NavigateToNode("descr_list",0))
	{
		UIConditionWnd					= new CUIConditionParams();
		UIConditionWnd->InitFromXml		(uiXml);
		UIWpnParams						= new CUIWpnParams();
		UIWpnParams->InitFromXml		(uiXml);
		// Only with a <weapon_params> layout; without it weapons keep the stock
		// condition bar and Lua-driven parameters.
		if (uiXml.NavigateToNode("weapon_params", 0))
		{
			UIWeaponParams				= new CUIWeaponParams();
			UIWeaponParams->InitFromXml	(uiXml);
		}
		UIKnifeParams					= new CUIKnifeParams();
		UIKnifeParams->InitFromXml		(uiXml);
		UIGrenadeParams					= new CUIGrenadeParams();
		UIGrenadeParams->InitFromXml	(uiXml);

		UIArtefactParams				= new CUIArtefactParams(CUIArtefactParams::CParamType::eParamTypeArtefact);
		UIArtefactParams->InitFromXml	(uiXml);

		UIOutfitParams = new CUIArtefactParams(CUIArtefactParams::CParamType::eParamTypeOutfit);
		UIOutfitParams->InitFromXml(uiXml);

		UIBackpackParams = new CUIArtefactParams(CUIArtefactParams::CParamType::eParamTypeBackpack);
		UIBackpackParams->InitFromXml(uiXml);

		if (uiXml.NavigateToNode("booster_params"))
		{
			UIBoosterInfo = new CUIBoosterInfo();
			UIBoosterInfo->InitFromXml(uiXml);
		}

		// Only with an <ammo_params> layout; without it ammo keeps the plain tooltip.
		if (uiXml.NavigateToNode("ammo_params", 0))
		{
			UIAmmoParams				= new CUIAmmoParams();
			UIAmmoParams->InitFromXml	(uiXml);
		}

		//UIDesc_line						= new CUIStatic();
		//AttachChild						(UIDesc_line);	
		//UIDesc_line->SetAutoDelete		(true);
		//xml_init.InitStatic				(uiXml, "description_line", 0, UIDesc_line);

		if ( ai().get_alife() ) // (-designer)
		{
			UIProperties					= new UIInvUpgPropertiesWnd();
			UIProperties->init_from_xml		("actor_menu_item.xml");

			// The window beside the tooltip (weapon upgrades with Shift): the list in
			// the tooltip's own frame, hidden until PlaceUpgradesSide shows it.
			UIUpgradesSide					= new UIInvUpgPropertiesWnd();
			UIUpgradesSide->init_from_xml	("actor_menu_item.xml");
			UIUpgradesSide->SetAutoDelete	(false);
			if (uiXml.NavigateToNode("background_frame", 0))
			{
				UIUpgradesFrame				= new CUIFrameWindow();
				UIUpgradesFrame->SetAutoDelete(false);
				AttachChild					(UIUpgradesFrame);
				xml_init.InitFrameWindow	(uiXml, "background_frame", 0, UIUpgradesFrame);
				UIUpgradesFrame->AttachChild(UIUpgradesSide);
				UIUpgradesFrame->Show		(false);
			}
		}

		UIDesc							= new CUIScrollView(); 
		AttachChild						(UIDesc);		
		UIDesc->SetAutoDelete			(true);
		m_desc_info.bShowDescrText		= !!uiXml.ReadAttribInt("descr_list",0,"only_text_info", 1);
		m_b_FitToHeight					= !!uiXml.ReadAttribInt("descr_list",0,"fit_to_height", 0);
		xml_init.InitScrollView			(uiXml, "descr_list", 0, UIDesc);
		xml_init.InitFont				(uiXml, "descr_list:font", 0, m_desc_info.uDescClr, m_desc_info.pDescFont);
	}	

	if (uiXml.NavigateToNode("image_static", 0))
	{	
		UIItemImage					= new CUI3dStatic();	 
		AttachChild					(UIItemImage);	
		UIItemImage->SetAutoDelete	(true);
		xml_init.InitStatic			(uiXml, "image_static", 0, UIItemImage);
		UIItemImage->TextureOn		();

		UIItemImage->TextureOff			();
		UIItemImageSize.set				(UIItemImage->GetWidth(),UIItemImage->GetHeight());
	}
	if ( uiXml.NavigateToNode( "outfit_info", 0 ) )
	{
		UIOutfitInfo				= new CUIOutfitInfo();
		UIOutfitInfo->InitFromXml	(uiXml);
	}

	xml_init.InitAutoStaticGroup	(uiXml, "auto", 0, this);
	BaseTooltipWidth = GetWidth();
	BaseDescriptionWidth = UIDesc ? UIDesc->GetWidth() : 0.0f;
	BaseNameWidth = UIName ? UIName->GetWidth() : 0.0f;
	BaseTradeTipWidth = UITradeTip ? UITradeTip->GetWidth() : 0.0f;
	BaseCostX = UICost ? UICost->GetWndPos().x : 0.0f;
	return true;
}

void CUIItemInfo::InitItemInfo(Fvector2 pos, Fvector2 size, const char* xml_name)
{
	inherited::SetWndPos	(pos);
	inherited::SetWndSize	(size);
	InitItemInfo			(xml_name);
}

bool	IsGameTypeSingle();

void CUIItemInfo::InitItem(CUICellItem* pCellItem, CInventoryItem* pCompareItem, u32 item_price, const char* trade_tip, bool overrideCorrectionByWeight)
{
	if(!pCellItem)
	{
		m_pInvItem			= nullptr;
		Enable				(false);
		return;
	}

	PIItem pInvItem			= (PIItem)pCellItem->m_pData;
	m_pInvItem				= pInvItem;
	Enable					(nullptr != m_pInvItem);
	if(!m_pInvItem)			return;

	if (UIDesc && m_b_FitToHeight)
	{
		UIDesc->SetWidth(BaseDescriptionWidth);
		float ExtraWidth = 0.0f;
		if (UIProperties && pInvItem->upgardes().size())
		{
			const float ContentWidth = UIDesc->GetDesiredChildWidth();
			UIProperties->SetAvailableWidth(ContentWidth, true);
			UIProperties->set_item_info(*pInvItem);
			ExtraWidth = std::max(0.0f, UIProperties->GetWidth() - ContentWidth);
		}
		// A weapon tooltip wider than the list's usable width (the stock layout leaves
		// 10 units on the right of the 290 list for the scroll bar): the room for the
		// difference column (ui-weapon-compare-numbers).
		if (UIWeaponParams && UIWeaponParams->Check(*pInvItem))
			ExtraWidth = std::max(ExtraWidth, UIWeaponParams->GetWidth() - (BaseDescriptionWidth - 10.0f));
		SetWidth(BaseTooltipWidth + ExtraWidth);
		UIDesc->SetWidth(BaseDescriptionWidth + ExtraWidth);
		if (UIName)
		{
			UIName->SetWidth(BaseNameWidth + ExtraWidth);
		}
		if (UITradeTip)
		{
			UITradeTip->SetWidth(BaseTradeTipWidth + ExtraWidth);
		}
		if (UICost)
		{
			Fvector2 CostPosition = UICost->GetWndPos();
			CostPosition.x = BaseCostX + ExtraWidth;
			UICost->SetWndPos(CostPosition);
		}
	}

	Fvector2				pos;	pos.set( 0.0f, 0.0f );
	string256				str;
	if ( UIName )
	{
		UIName->SetText((pInvItem->IsUsedAdditionalName() || pInvItem->IsUsedPrependName()) ? pInvItem->GetExtendedUnionName().c_str() : pInvItem->NameItem());
		UIName->AdjustHeightToText();
		pos.y = UIName->GetWndPos().y + UIName->GetHeight() + 4.0f;
	}
	if ( UIWeight )
	{
		const char*  kg_str = g_pStringTable->translate( "st_kg" ).c_str();
		// Whole stack, like the count on the cell: ammo boxes under one cell hold
		// different counts, so the top one alone says little. Helpers aren't carried.
		auto	item_weight = [](PIItem item) { return item->m_pInventory ? item->m_pInventory->CalcItemWeight(item) : item->Weight(); };
		float	weight	= 0.0f;
		bool	counted	= false;
		if ( !pCellItem->IsHelper() )
		{
			weight		+= item_weight(pInvItem);
			counted		= true;
		}
		for ( u32 j = 0; j < pCellItem->ChildsCount(); ++j )
		{
			CUICellItem* child = pCellItem->Child(j);
			if ( child->IsHelper() )
				continue;
			weight		+= item_weight((PIItem)child->m_pData);
			counted		= true;
		}
		if ( !counted )
			weight		= item_weight(pInvItem);

		if ( !weight )
		{
			if ( CWeaponAmmo* ammo = dynamic_cast<CWeaponAmmo*>(pInvItem) )
			{
				// its helper item, m_boxCur is zero, so recalculate via CInventoryItem::Weight()
				weight = pInvItem->m_pInventory ? pInvItem->m_pInventory->CalcItemWeight(pInvItem) : pInvItem->CInventoryItem::Weight();
				for( u32 j = 0; j < pCellItem->ChildsCount(); ++j )
				{
					PIItem jitem	= (PIItem)pCellItem->Child(j)->m_pData;
					weight += jitem->m_pInventory ? jitem->m_pInventory->CalcItemWeight(jitem) : jitem->CInventoryItem::Weight();
				}

			}
		}

		if (CInventoryVolumeSystem::Get().IsEnabled())
		{
			xr_sprintf(str, "%3.2f %s | V %3.2f", weight, kg_str, CInventoryVolumeSystem::Get().GetItemVolume(*pInvItem));
		}
		else
		{
			xr_sprintf(str, "%3.2f %s", weight, kg_str);
		}
		UIWeight->SetText	(str);
		
		pos.x = UIWeight->GetWndPos().x;
		if ( m_complex_desc )
		{
			UIWeight->SetWndPos	(pos);
		}
	}

	if (UICost)
	{
		if (IsGameTypeSingleCompatible() && item_price != u32(-1) && pInvItem->IsDrawCost())
		{
			xr_sprintf(str, "%u RU", item_price);// will be owerwritten in multiplayer
			UICost->SetText(str);
			pos.x = UICost->GetWndPos().x;
			if (m_complex_desc)
			{
				UICost->SetWndPos(pos);
			}
			UICost->Show(true);
		}
		else
		{
			UICost->Show(false);
		}
	}

	if (UICondProgresBar)
	{
		float cond = pInvItem->GetConditionToShow();
		UICondProgresBar->Show(true);
		UICondProgresBar->SetProgressPos(cond * 100.0f + 1.0f - EPS);
	}

	if ( UITradeTip && IsGameTypeSingleCompatible())
	{
		pos.y = UITradeTip->GetWndPos().y;
		if ( UIWeight && m_complex_desc )
		{
			pos.y = UIWeight->GetWndPos().y + UIWeight->GetHeight() + 4.0f;
		}

		if(trade_tip==nullptr)
			UITradeTip->Show(false);
		else
		{
			UITradeTip->SetText(g_pStringTable->translate(trade_tip).c_str());
			UITradeTip->AdjustHeightToText();
			UITradeTip->SetWndPos(pos);
			UITradeTip->Show(true);
		}
	}
	
	if ( UIDesc )
	{
		pos = UIDesc->GetWndPos();
		if ( UIWeight && !overrideCorrectionByWeight)
			pos.y = UIWeight->GetWndPos().y + UIWeight->GetHeight() + 4.0f;

		if(UITradeTip && trade_tip!=nullptr)
			pos.y = UITradeTip->GetWndPos().y + UITradeTip->GetHeight() + 4.0f;

		UIDesc->SetWndPos		(pos);
		UIDesc->Clear			();
		VERIFY					(0==UIDesc->GetSize());
		if(m_desc_info.bShowDescrText)
		{
			CUIStatic* pItem					= new CUIStatic();
			pItem->SetTextColor					(m_desc_info.uDescClr);
			pItem->SetFont						(m_desc_info.pDescFont);
			pItem->SetWidth						(UIDesc->GetDesiredChildWidth());
			pItem->SetTextComplexMode			(true);
			pItem->SetText((pInvItem->IsUsedPrependDescription() || pInvItem->IsUsedAdditionalDescription()) ? *pInvItem->GetExtendedUnionDescription() : *pInvItem->ItemDescription());
			pItem->AdjustHeightToText			();
			UIDesc->AddWindow					(pItem, true);
		}
		TryAddConditionInfo					(*pInvItem, pCompareItem);
		TryAddWpnInfo						(*pInvItem, pCompareItem);
		TryAddKnifeInfo						(*pInvItem, pCompareItem);
		TryAddGrenadeInfo					(*pInvItem, pCompareItem);
		TryAddArtefactInfo					(*pInvItem);
		TryAddOutfitInfo					(*pInvItem, pCompareItem);
		TryAddUpgradeInfo					(*pInvItem);
		TryAddBoosterInfo					(*pInvItem);
		TryAddAmmoInfo						(*pInvItem);

		if(m_b_FitToHeight)
		{
			UIDesc->SetWndSize				(Fvector2().set(UIDesc->GetWndSize().x, UIDesc->GetPadSize().y) );
			Fvector2 new_size;
			new_size.x						= GetWndSize().x;
			new_size.y						= UIDesc->GetWndPos().y+UIDesc->GetWndSize().y+20.0f;
			new_size.x						= std::max(105.0f, new_size.x);
			new_size.y						= std::max(105.0f, new_size.y);
			
			SetWndSize						(new_size);
			if(UIBackground)
				UIBackground->SetWndSize	(new_size);
		}

		UIDesc->ScrollToBegin				();
	}
	if(UIItemImage)
    {
		static const bool enable3DIcons = EngineExternal()[EEngineExternalGame::Enable3DIcons];
		if (enable3DIcons)
        {
            UIItemImage->SetVisual(pInvItem->m_3d_static_visual_name);
            UIItemImage->SetScaleFactor(pInvItem->m_3d_static_scale);
            UIItemImage->SetIconOffset(pInvItem->IconOffset);
            UIItemImage->SetXYZ(pInvItem->m_3d_static_rotate);
			UIItemImage->SetBonesVisible(pInvItem->object().Visual()->dcast_PKinematics());
        }
        else
            UIItemImage->SetVisual(nullptr);

        // Р—Р°РіСЂСѓР¶Р°РµРј РєР°СЂС‚РёРЅРєСѓ
		UIItemImage->SetShader(InventoryUtilities::GetEquipmentIconsShader(m_pInvItem->IconsTexture.c_str()));

		Irect item_grid_rect = pInvItem->GetInvGridRect();
		float scaleIcon = m_pInvItem->ScaleIcon;
		Frect texture_rect = {};
		texture_rect.lt.set(item_grid_rect.x1 * INV_GRID_WIDTH(scaleIcon), item_grid_rect.y1 * INV_GRID_HEIGHT(scaleIcon));
		texture_rect.rb.set(item_grid_rect.x2 * INV_GRID_WIDTH(scaleIcon), item_grid_rect.y2 * INV_GRID_HEIGHT(scaleIcon));
		texture_rect.rb.add(texture_rect.lt);
		UIItemImage->GetUIStaticItem().SetTextureRect(texture_rect);
		UIItemImage->TextureOn				();
		UIItemImage->SetStretchTexture		(true);

		Fvector2 v_r = {};

		v_r = { item_grid_rect.x2 * INV_GRID_WIDTH2(scaleIcon) / scaleIcon,
			item_grid_rect.y2 * INV_GRID_HEIGHT2(scaleIcon) / scaleIcon };

		v_r.x								*= UI().get_current_kx();


		UIItemImage->GetUIStaticItem().SetSize	(v_r);
		UIItemImage->SetWidth					(v_r.x);
		UIItemImage->SetHeight					(v_r.y);
    }
}

void CUIItemInfo::TryAddConditionInfo(CInventoryItem& pInvItem, CInventoryItem* pCompareItem)
{
	// The weapon tooltip has its own condition row.
	if (pInvItem.IsUsingCondition() && !(UIWeaponParams && UIWeaponParams->Check(pInvItem)))
	{
		UIConditionWnd->SetInfo(pCompareItem, pInvItem);
		UIDesc->AddWindow(UIConditionWnd, false);
	}
	else
	{
		UIDesc->RemoveWindow(UIConditionWnd);
	}
}

void CUIItemInfo::TryAddWpnInfo(CInventoryItem& pInvItem, CInventoryItem* pCompareItem)
{
	if (UIWeaponParams && UIWeaponParams->Check(pInvItem))
	{
		UIWeaponParams->SetInfo(pCompareItem, pInvItem);
		UIDesc->AddWindow(UIWeaponParams, false);
	}
	else if (UIWpnParams->Check(pInvItem))
	{
		UIWpnParams->SetInfo(pCompareItem, pInvItem);
		UIDesc->AddWindow(UIWpnParams, false);
	}
}

void CUIItemInfo::TryAddKnifeInfo(CInventoryItem& pInvItem, CInventoryItem* pCompareItem)
{
	if (UIKnifeParams->Check(pInvItem))
	{
		UIKnifeParams->SetInfo(pCompareItem, pInvItem);
		UIDesc->AddWindow(UIKnifeParams, false);
	}
}

void CUIItemInfo::TryAddGrenadeInfo(CInventoryItem& pInvItem, CInventoryItem* pCompareItem)
{
	if (UIGrenadeParams->Check(pInvItem))
	{
		UIGrenadeParams->SetInfo(pCompareItem, pInvItem);
		UIDesc->AddWindow(UIGrenadeParams, false);
	}
}

void CUIItemInfo::TryAddArtefactInfo	(CInventoryItem& pInvItem)
{
	if (UIArtefactParams->Check(pInvItem.object().cNameSect()))
	{
		UIArtefactParams->SetInfo(pInvItem);
		UIDesc->AddWindow(UIArtefactParams, false);
	}
}

void CUIItemInfo::TryAddOutfitInfo(CInventoryItem& pInvItem, CInventoryItem* pCompareItem)
{
	CCustomOutfit* outfit = pInvItem.cast_outfit();
	CHelmet* helmet = pInvItem.cast_helmet();
	CBackpack* backpack = pInvItem.cast_backpack();

	if (outfit && UIOutfitInfo)
	{
		CCustomOutfit* comp_outfit = pCompareItem ? pCompareItem->cast_outfit() : nullptr;
		UIOutfitInfo->UpdateInfo(outfit, comp_outfit);
		UIDesc->AddWindow(UIOutfitInfo, false);
	}

	if (helmet && UIOutfitInfo)
	{
		CHelmet* comp_helmet = pCompareItem ? pCompareItem->cast_helmet() : nullptr;
		UIOutfitInfo->UpdateInfo(helmet, comp_helmet);
		UIDesc->AddWindow(UIOutfitInfo, false);
	}

	if (UIOutfitParams && outfit)
	{
		UIOutfitParams->SetInfo(pInvItem);
		UIDesc->AddWindow(UIOutfitParams, false);
	}

	if (UIBackpackParams && backpack)
	{
		UIBackpackParams->SetInfo(pInvItem);
		UIDesc->AddWindow(UIBackpackParams, false);
	}
}

void CUIItemInfo::SetUpgradesSide(bool enabled)
{
	m_upgrades_side = enabled && UIUpgradesSide && UIUpgradesFrame;
	if (UIProperties)
		UIProperties->set_list_in_side(m_upgrades_side);
	if (!m_upgrades_side && UIUpgradesFrame)
		UIUpgradesFrame->Show(false);
}

// The weapon's upgrades in a window of their own beside the tooltip, only while Shift
// is held: the tooltip keeps its height, so it does not jump when the list appears.
// To the right of the tooltip when there is room in vis_rect (the tooltip's own
// coordinates, as for fit_in_rect), otherwise to the left; level with its top and
// kept inside vis_rect vertically.
void CUIItemInfo::PlaceUpgradesSide(const Frect& vis_rect)
{
	if (!UIUpgradesFrame)
		return;
	UIUpgradesFrame->Show(false);
	if (!m_upgrades_side || !m_pInvItem || m_pInvItem->upgardes().empty() || !UIDesc || !UIProperties ||
		!UIProperties->hint_applies(*m_pInvItem) || pInput->GetControllerMode() ||
		!(pInput->iGetAsyncKeyState(SDL_SCANCODE_LSHIFT) || pInput->iGetAsyncKeyState(SDL_SCANCODE_RSHIFT)))
		return;

	constexpr float MarginX = 16.0f;		// as the tooltip's text margins
	constexpr float MarginY = 14.0f;
	constexpr float Gap = 4.0f;
	UIUpgradesSide->SetAvailableWidth(UIDesc->GetDesiredChildWidth(), true);
	UIUpgradesSide->set_item_list(*m_pInvItem);
	UIUpgradesSide->SetWndPos(Fvector2().set(MarginX, MarginY));
	Fvector2 size = UIUpgradesSide->GetWndSize();
	// Not smaller than the frame's corner textures (the tooltip keeps 105 as well).
	size.x = std::max(105.0f, size.x + 2.0f * MarginX);
	size.y = std::max(105.0f, size.y + 2.0f * MarginY);
	UIUpgradesFrame->SetWndSize(size);

	const Fvector2 pos = GetWndPos();
	const Fvector2 own = GetWndSize();
	const bool right = pos.x + own.x + Gap + size.x <= vis_rect.x2 || pos.x - Gap - size.x < vis_rect.x1;
	const float x = right ? own.x + Gap : -(size.x + Gap);
	const float top = std::max(vis_rect.y1, std::min(pos.y, vis_rect.y2 - size.y));
	UIUpgradesFrame->SetWndPos(Fvector2().set(x, top - pos.y));
	UIUpgradesFrame->Show(true);
}

void CUIItemInfo::TryAddUpgradeInfo(CInventoryItem& pInvItem)
{
	if (pInvItem.upgardes().size() && UIProperties)
	{
		if (!m_b_FitToHeight)
		{
			UIProperties->SetAvailableWidth(UIDesc->GetDesiredChildWidth(), false);
			UIProperties->set_item_info(pInvItem);
		}
		UIDesc->AddWindow(UIProperties, false);
	}
}

void CUIItemInfo::TryAddBoosterInfo(CInventoryItem& pInvItem)
{
	CEatableItem* food = pInvItem.cast_eatable_item();
	if (food && UIBoosterInfo)
	{
		UIBoosterInfo->SetInfo(pInvItem.object().cNameSect());
		UIDesc->AddWindow(UIBoosterInfo, false);
	}
}

void CUIItemInfo::TryAddAmmoInfo(CInventoryItem& pInvItem)
{
	if (UIAmmoParams && UIAmmoParams->Check(pInvItem))
	{
		UIAmmoParams->SetInfo(pInvItem.object().cNameSect());
		UIDesc->AddWindow(UIAmmoParams, false);
	}
}

void CUIItemInfo::Draw()
{
	if (ShouldHide3DIconItemDescriptions())
	{
		return;
	}
	if(m_pInvItem)
		inherited::Draw();
}

void CUIItemInfo::ScrollDown()
{
	if (UIDesc && !m_b_FitToHeight)
	{
		if (CUIScrollBar* s = UIDesc->ScrollBar())
		{
			s->TryScrollInc();
		}
	}
}

void CUIItemInfo::ScrollUp()
{
	if (UIDesc && !m_b_FitToHeight)
	{
		if (CUIScrollBar* s = UIDesc->ScrollBar())
		{
			s->TryScrollDec();
		}
	}
}
