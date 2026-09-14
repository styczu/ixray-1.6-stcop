#pragma once

#include "../../xrUI/Widgets/UIWindow.h"
#include "../../xrUI/Widgets/UIWndCallback.h"

class CUICellContainer;
class CUIScrollBar;
class CUIStatic;
class CUICellItem;
class CUIDragItem;


struct CUICell{
							CUICell					()						{m_item=NULL; Clear();}

		CUICellItem*		m_item;
		bool				m_bMainItem;

		void				SetItem					(CUICellItem* itm, bool bMain)		{m_item = itm; VERIFY(m_item);m_bMainItem = bMain;}
		bool				Empty					()						{return m_item == NULL;}
		bool				MainItem				()						{return m_bMainItem;}
		void				Clear					();
		bool				operator ==				(const CUICell& C) const{return (m_item == C.m_item);}
};

typedef xr_vector<CUICell>			UI_CELLS_VEC;
typedef UI_CELLS_VEC::iterator		UI_CELLS_VEC_IT;

// What dropping an item at a given position does: merge it into a similar stack,
// place it at the cell it points at, or hand it to automatic placement.
enum EDropPreview
{
	dpMerge,
	dpPlace,
	dpAuto,
};

class CUIDragDropListEx :public CUIWindow, public CUIWndCallback
{
private:
	typedef CUIWindow inherited;

	enum{	
		flGroupSimilar		=	(1<<0),
		flAutoGrow			=	(1<<1),
		flCustomPlacement	=	(1<<2),
		flVerticalPlacement	=	(1<<3),
		flAlwaysShowScroll	=	(1<<4),
		flVirtualCells		=	(1<<5),
	};
	Flags8					m_flags;
	Ivector2				m_orig_cell_capacity;
	Ivector2				m_virtual_cells_alignment;
	bool					m_bConditionProgBarVisible;
protected:
	CUICellItem*			m_selected_item;
	CUICellContainer*		m_container;
	CUIScrollBar*			m_vScrollBar;

	virtual void			OnScrollV				(CUIWindow* w, void* pData);
	virtual void			OnItemStartDragging		(CUIWindow* w, void* pData);
	virtual void			OnItemDrop				(CUIWindow* w, void* pData);
	virtual void			OnItemSelected			(CUIWindow* w, void* pData);
	virtual void			OnItemLButtonClick		(CUIWindow* w, void* pData);
	virtual void			OnItemRButtonClick		(CUIWindow* w, void* pData);
	virtual void			OnItemDBClick			(CUIWindow* w, void* pData);
	virtual void			OnItemFocusReceived		(CUIWindow* w, void* pData);
	virtual void			OnItemFocusLost			(CUIWindow* w, void* pData);
	virtual void			OnItemFocusedUpdate		(CUIWindow* w, void* pData);
	
public:
	static CUIDragItem*		m_drag_item;
							CUIDragDropListEx	();
	virtual					~CUIDragDropListEx	();
				void		InitDragDropList		(Fvector2 pos, Fvector2 size);

	typedef					xr_delegate<bool(CUICellItem*)>			DRAG_CELL_EVENT;
	typedef					xr_delegate<void(CUIDragItem*, bool)>	DRAG_ITEM_EVENT;

	DRAG_CELL_EVENT			m_f_item_drop;
	DRAG_CELL_EVENT			m_f_item_start_drag;
	DRAG_CELL_EVENT			m_f_item_db_click;
	DRAG_CELL_EVENT			m_f_item_selected;
	DRAG_CELL_EVENT			m_f_item_lbutton_click;
	DRAG_CELL_EVENT			m_f_item_rbutton_click;
	DRAG_CELL_EVENT			m_f_item_focus_received;
	DRAG_CELL_EVENT			m_f_item_focus_lost;
	DRAG_CELL_EVENT			m_f_item_focused_update;
	DRAG_ITEM_EVENT			m_f_drag_event;

	u32						back_color;

	const	Ivector2&		CellsCapacity		();
			void			SetCellsCapacity	(const Ivector2 c);
			void			SetStartCellsCapacity(const Ivector2 c){m_orig_cell_capacity=c;SetCellsCapacity(c);};
			void			ResetCellsCapacity	(){VERIFY(ItemsCount()==0);SetCellsCapacity(m_orig_cell_capacity);};
	 const	Fvector2&		CellSize			();
			void			SetCellSize			(const Ivector2 new_sz);
	const	Fvector2&		CellsSpacing		();
			void			SetCellsSpacing		(const Ivector2& new_sz);
			void			SetCellsVertAlignment(xr_string alignment);
			void			SetCellsHorizAlignment(xr_string alignment);

	const	Ivector2		GetVirtualCellsAlignment() {return m_virtual_cells_alignment;};

			int				ScrollPos			();
			void			ReinitScroll		();
			void			GetClientArea		(Frect& r);
			Fvector2		GetDragItemPosition	();

			void			SetAutoGrow			(bool b);
			bool			IsAutoGrow			();
			void			SetGrouping			(bool b);
			bool			IsGrouping			();
			void			SetCustomPlacement	(bool b);
			bool			GetCustomPlacement	();
			void			SetVerticalPlacement(bool b);
			bool			GetVerticalPlacement();
			void			SetAlwaysShowScroll	(bool b);
			bool			GetVirtualCells		();
			void			SetVirtualCells		(bool b);

			bool			GetConditionProgBarVisibility() {return m_bConditionProgBarVisible;};
			void			SetConditionProgBarVisibility(bool b) {m_bConditionProgBarVisible = b;};
public:
			// items management
			virtual void	SetItem				(CUICellItem* itm); //auto
			virtual bool	SetItem				(CUICellItem* itm, Fvector2 abs_pos);  // start at cursor pos
			virtual void	SetItem				(CUICellItem* itm, Ivector2 cell_pos); // start at cell
	virtual EDropPreview	PredictDrop			(CUICellItem* itm, const Fvector2& abs_pos, Irect& out_cells, CUICellItem* skip = nullptr);
					bool	CanSetItem			(CUICellItem* itm);
			
			u32				ItemsCount			();
			CUICellItem*	GetItemIdx			(u32 idx);
	virtual CUICellItem*	RemoveItem			(CUICellItem* itm, bool force_root);
			void			CreateDragItem		(CUICellItem* itm);

			void			DestroyDragItem		();
			void			ClearAll(bool bDestroy, xr_vector<u16> IgnoredItemsIds = {}); // FFx0001
			void			Compact				();
			bool			IsOwner				(CUICellItem* itm);
			void			clear_select_armament();
			Ivector2		PickCell			(const Fvector2& abs_pos);
			CUICell&		GetCellAt			(const Ivector2& pos);
			CUICellContainer* GetContainer		() { return m_container; }; //Alundaio

public:
	//UIWindow overriding
	virtual		void		Draw				();
	virtual		void		Update				();
	virtual		bool		OnMouseAction		(float x, float y, EUIMessages mouse_action);
	virtual		void		SendMessage			(CUIWindow* pWnd, s16 msg, void* pData = NULL);

				void		OnDragEvent			(CUIDragItem* drag_item, bool b_receive);

	virtual CUIWindow* ui_cast_window() { return this; }
};

class CUICellContainer :public CUIWindow
{
	friend class CUIDragDropListEx;
	friend class CUIDragDropReferenceList;

private:
	typedef CUIWindow inherited;
	ui_shader					hShader;
	UI_CELLS_VEC				m_cells_to_draw;
protected:
	CUIDragDropListEx*			m_pParentDragDropList;
	bool						m_isInventoryGridDisabled;

	Ivector2					m_cellsCapacity;			//count		(col,	row)

	// One cell has to cover a whole number of screen pixels or neighbouring items
	// gape and overlap by one after rasterization. The screen sizes are the truth;
	// the UI-base ones are only what CUIWindow positions have to be expressed in.
	Ivector2					m_cellSizeRaw;				//UI base	(width, height) as read from XML
	Ivector2					m_cellSpacingRaw;			//UI base	(width, height) as read from XML
	Ivector2					m_cellSizeScreen;			//screen px	(width, height) whole pixels
	Ivector2					m_cellSpacingScreen;		//screen px	(width, height) whole pixels
	Fvector2					m_cellSize;					//UI base	m_cellSizeScreen / scale
	Fvector2					m_cellSpacing;				//UI base	m_cellSpacingScreen / scale
	Fvector2					m_metricsScale;				//scale the four above were built at

	UI_CELLS_VEC				m_cells;

	void						GetTexUVLT			(Fvector2& uv, u32 col, u32 row, u8 select_mode);
	void						ReinitSize			();
	u32							GetCellsInRange		(const Irect& rect, UI_CELLS_VEC& res);

public:							
								CUICellContainer	(CUIDragDropListEx* parent);
	virtual						~CUICellContainer	();
				CUICell&		GetCellAt			(const Ivector2& pos);
				Ivector2		PickCell			(const Fvector2& abs_pos);
				bool			ValidCell			(const Ivector2& pos) const;

	virtual CUIWindow* ui_cast_window() { return this; }

protected:
	virtual		void			Draw				();
	virtual		void			Update				();
				void			DrawDropPreview		(const Irect& tgt_cells, const Fvector2& draw_lt, const Fvector2& f_len, const Fvector2& sp_len);

	IC const	Ivector2&		CellsCapacity		()								{return m_cellsCapacity;};	
				void			SetCellsCapacity	(const Ivector2& c);
	IC const	Fvector2&		CellSize			()								{return m_cellSize;};	
				void			SetCellSize			(const Ivector2& new_sz);
	IC const	Fvector2&		CellsSpacing		()								{return m_cellSpacing;};	
				void			SetCellsSpacing		(const Ivector2& new_sz);
	IC const	Ivector2&		CellSizeScreen		()								{return m_cellSizeScreen;};
	IC const	Ivector2&		CellsSpacingScreen	()								{return m_cellSpacingScreen;};

				// The single place the grid step enters geometry: offset of a cell's top
				// left corner from the container origin, in UI base units.
				Fvector2		CellOffsetUI		(const Ivector2& cell_pos) const;
				// Rebuilds the four metrics above from the current UI scale. True when
				// they changed, which means the caller owes a ReinitSize + RefreshItemsPos.
				bool			UpdateCellMetrics	();
				void			RefreshItemsPos		();
				Ivector2		TopVisibleCell		();
				Ivector2		GetItemPos			(CUICellItem* itm);
				Ivector2		FindFreeCell		(const Ivector2& size);
				bool			HasFreeSpace		(const Ivector2& size);
				bool			IsRoomFree			(const Ivector2& pos, const Ivector2& size, const CUICellItem* ignore = nullptr);
				
				bool			AddSimilar			(CUICellItem* itm);
				CUICellItem*	FindSimilar			(CUICellItem* itm, CUICellItem* skip = nullptr);

				void			PlaceItemAtPos		(CUICellItem* itm, Ivector2& cell_pos);
				void			SetItemGeometry		(CUICellItem* itm, const Ivector2& cell_pos);
				CUICellItem*	RemoveItem			(CUICellItem* itm, bool force_root);

				void			Grow				();
				void			Shrink				();
				void			ClearAll			(bool bDestroy, xr_vector<u16> IgnoredItemsIds = {}); // FFx0001
				void			clear_select_armament();


};
