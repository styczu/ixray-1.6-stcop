#pragma once
#include "UIDragDropListEx.h"
#include "../../xrUI/Widgets/UI3dStatic.h"
#include "../../xrServerEntities/inventory_space.h"
class CInventoryOwner;

class CUIDragDropReferenceList final : public CUIDragDropListEx
{
private:
	typedef CUIDragDropListEx inherited;
	typedef xr_vector<CUI3dStatic*> ITEMS_REFERENCES_VEC;
	typedef ITEMS_REFERENCES_VEC::iterator ITEMS_REFERENCES_VEC_IT;
	ITEMS_REFERENCES_VEC m_references;
	// Scale and container position the reference icons were laid out at; they sit on
	// the container cells, which move when the screen scale changes.
	Fvector2 m_references_scale;
	Fvector2 m_references_origin;
	// Grid height of the icon in each reference: a slot is one column of the list and
	// an icon shorter than it is centred down it, like the cell item under it.
	xr_vector<int> m_reference_rows;

	void PlaceReferences();
	void PlaceReference(u32 index);
	void SetReferenceRows(u32 index, int rows);
	// Every item goes to the top cell of the slot's column.
	Ivector2 SlotCell(const Ivector2& cell_pos) const { return Ivector2().set(cell_pos.x, 0); }

public:
	CUIDragDropReferenceList();
	virtual ~CUIDragDropReferenceList();
	virtual void SetItem(CUICellItem* itm);
	virtual bool SetItem(CUICellItem* itm, Fvector2 abs_pos);
	virtual void SetItem(CUICellItem* itm, Ivector2 cell_pos);
	bool			SetItemAtQuickSlotCell	(CUICellItem* itm, u8 slotIndex);
	virtual EDropPreview PredictDrop(CUICellItem* itm, const Fvector2& abs_pos, Irect& out_cells, CUICellItem* skip = nullptr);
	virtual CUICellItem* RemoveItem(CUICellItem* itm, bool force_root);

	void Initialize();
	virtual void Update();
	CUICellContainer* GetContainer() {return m_container;};
	// One cell wide and no taller than the slot: rows_num of the list, so a one-row
	// layout keeps the quick slots to 1x1 items.
	bool CanHoldGridSize(const Ivector2& size) { return size.x <= 1 && size.y <= m_container->CellsCapacity().y; }
	void LoadItemTexture(const char* section, Ivector2 cell_pos);
	void ReloadReferences(CInventoryOwner* pActor);

	virtual void 	OnItemDBClick		(CUIWindow* w, void* pData);
	virtual void 	OnItemDrop			(CUIWindow* w, void* pData);

	virtual CUIWindow* ui_cast_window() { return this; }
};