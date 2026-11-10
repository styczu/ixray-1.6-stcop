#pragma once

#include "../xrCore/_stl_extensions_nonalloc.h"
#include "../xrServerEntities/inventory_space.h"

inline constexpr u16 kWeaponCycleNoGameAction = u16(-1);

bool InventorySecondarySlotPairingStrict();

inline bool IsSidearmPhysicalSlot(u16 slot)
{
	return slot == INV_SLOT_2 || slot == PISTOL_SLOT_NEW;
}

inline bool IsPrimaryPhysicalSlot(u16 slot)
{
	return slot == INV_SLOT_3;
}

inline bool IsSidearmOrPrimaryPhysicalSlot(u16 slot)
{
	return IsSidearmPhysicalSlot(slot) || IsPrimaryPhysicalSlot(slot);
}

u16 InventoryWeaponHotkeyToInventorySlot(u16 gameActionCmd);

class CInventory;
class CInventoryItem;
bool InventoryAnySidearmCellOccupied(const CInventory& inv);

bool InventoryHolsterPistolSlotActiveInSettings();

bool InventoryHolsterExclusivePistolFootprint(CInventoryItem* item);

u16 InventoryResolveSidearmEquipSlot(CInventoryItem* item);

// Stalker smart covers need a long gun. Besides INV_SLOT_3 weapons this accepts
// sidearm-slot weapons with long-gun animations (animation_slot != 1), e.g.
// sawn-offs moved to the pistol holster with slot = 1.
bool StalkerWeaponFitsSmartCover(CInventoryItem* item);

xr_span<const u16> InventoryWeaponCycleSlots();
u16 InventoryWeaponSlotToGameAction(u16 slotId);
void InventoryWeaponCycleInvalidate();

