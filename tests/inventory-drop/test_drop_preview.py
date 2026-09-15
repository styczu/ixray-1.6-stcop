"""Compile the production drop prediction and the production cell drawing, then check
that the cells highlighted while dragging are the cells SetItem would fill, and that
the highlight is painted on exactly the same pixels as the grid cell underneath."""
from pathlib import Path
import os
import re
import struct
import subprocess
import tempfile

root = Path(os.environ.get('IXRAY_TEST_ROOT', str(Path(__file__).resolve().parents[2])))


def body(text, signature):
    start = text.index('{', text.index(signature))
    depth, end = 1, start + 1
    while depth:
        if text[end] == '{':
            depth += 1
        elif text[end] == '}':
            depth -= 1
        end += 1
    return text[start:end]


drag_drop = (root / 'src/xrGame/ui/UIDragDropListEx.cpp').read_text()
reference_list = (root / 'src/xrGame/ui/UIDragDropReferenceList.cpp').read_text()

# The cell tints and the UV span live in the anonymous namespace at the top of the file.
constants = '\n'.join(re.findall(r'^constexpr (?:float|u32) k\w+\s*=.*?;$', drag_drop, re.M))
assert 'kDropPreviewFree' in constants and 'kInventoryCellUSpanGridDisabled' in constants, constants

# ui_grid_alt hides the normal inventory grid by making its first 64-pixel strip
# fully transparent. The preview must therefore use the next, visible neutral strip
# as a mask. Pin that asset contract here as well as the production UV selection.
grid_alt = (root / 'gamedata/textures/ui/ui_grid_alt.dds').read_bytes()
assert grid_alt[:4] == b'DDS ' and grid_alt[84:88] == b'DXT5'
grid_height, grid_width = struct.unpack_from('<II', grid_alt, 12)
assert (grid_width, grid_height) == (256, 64)
blocks_per_row = (grid_width + 3) // 4


def dxt5_alpha_endpoints(x1, x2):
    for block_y in range((grid_height + 3) // 4):
        for block_x in range(x1 // 4, x2 // 4):
            offset = 128 + (block_y * blocks_per_row + block_x) * 16
            yield grid_alt[offset], grid_alt[offset + 1]


assert all(a0 == 0 and a1 == 0 for a0, a1 in dxt5_alpha_endpoints(0, 64))
assert all(a0 == 102 and a1 == 102 for a0, a1 in dxt5_alpha_endpoints(64, 128))
assert round(240 * 102 / 255) == 96

bodies = {
    'PREDICT_BODY': body(drag_drop, 'SDropPrediction CUIDragDropListEx::PredictDrop('),
    'PICK_BODY': body(drag_drop, 'Ivector2 CUICellContainer::PickCell('),
    'VALID_BODY': body(drag_drop, 'bool CUICellContainer::ValidCell('),
    'ROOM_BODY': body(drag_drop, 'bool CUICellContainer::IsRoomFree('),
    'ROOM_CAPACITY_BODY': body(drag_drop, 'bool CUICellContainer::IsRoomFree(const Ivector2& pos, const Ivector2& _size, const Ivector2& capacity'),
    'FIND_CAPACITY_BODY': body(drag_drop, 'bool CUICellContainer::FindFreeCellInCapacity('),
    'RESOLVE_BODY': body(drag_drop, 'bool CUICellContainer::ResolveFreeCell('),
    'FIND_BODY': body(drag_drop, 'Ivector2 CUICellContainer::FindFreeCell('),
    'GROW_BODY': body(drag_drop, 'void CUICellContainer::Grow('),
    'BODY_LIST_SET_AUTO': body(drag_drop, 'void CUIDragDropListEx::SetItem(CUICellItem* itm) //auto'),
    'BODY_LIST_SET_ABS': body(drag_drop, 'bool CUIDragDropListEx::SetItem(CUICellItem* itm, Fvector2 abs_pos)'),
    'BODY_LIST_SET_CELL': body(drag_drop, 'void CUIDragDropListEx::SetItem(CUICellItem* itm, Ivector2 cell_pos)'),
    'BODY_LIST_REMOVE': body(drag_drop, 'CUICellItem* CUIDragDropListEx::RemoveItem('),
    'BODY_ADD_SIMILAR': body(drag_drop, 'bool CUICellContainer::AddSimilar('),
    'BODY_PLACE_ITEM': body(drag_drop, 'void CUICellContainer::PlaceItemAtPos('),
    'BODY_CONTAINER_REMOVE': body(drag_drop, 'CUICellItem* CUICellContainer::RemoveItem('),
    'SIMILAR_BODY': body(drag_drop, 'CUICellItem* CUICellContainer::FindSimilar('),
    'CELL_AT_BODY': body(drag_drop, 'CUICell& CUICellContainer::GetCellAt('),
    'ITEM_POS_BODY': body(drag_drop, 'Ivector2 CUICellContainer::GetItemPos('),
    'TEX_UV_BODY': body(drag_drop, 'void CUICellContainer::GetTexUVLT('),
    'TOP_CELL_BODY': body(drag_drop, 'Ivector2 CUICellContainer::TopVisibleCell('),
    'IN_RANGE_BODY': body(drag_drop, 'u32 CUICellContainer::GetCellsInRange('),
    'DRAW_BODY': body(drag_drop, 'void CUICellContainer::Draw('),
    'PREVIEW_BODY': body(drag_drop, 'void CUICellContainer::DrawDropPreview('),
    'METRICS_BODY': body(drag_drop, 'bool CUICellContainer::UpdateCellMetrics('),
    'OFFSET_BODY': body(drag_drop, 'Fvector2 CUICellContainer::CellOffsetUI('),
    'SCREEN_LEN_BODY': body(drag_drop, 'IC int screen_cell_len('),
    'SNAP_GRID_BODY': body(drag_drop, 'IC float snap_grid_px('),
    'REF_DROP_BODY': body(reference_list, 'SDropPrediction CUIDragDropReferenceList::PredictDrop('),
    'REF_SET_ABS_BODY': body(reference_list, 'bool CUIDragDropReferenceList::SetItem(CUICellItem* itm, Fvector2 abs_pos)'),
}

code = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include <utility>
#include <algorithm>

#define R_ASSERT(x) assert(x)
#define R_ASSERT2(x, message) assert(x)
typedef unsigned int u32;
typedef unsigned char u8;
static const float EPS = 0.0000001f;
constexpr u32 color_rgba(u32 r, u32 g, u32 b, u32 a) { return ((a & 0xffu) << 24) | ((b & 0xffu) << 16) | ((g & 0xffu) << 8) | (r & 0xffu); }
constexpr u32 subst_alpha(u32 rgba, u32 a) { return (rgba & 0x00ffffffu) | color_rgba(0, 0, 0, a); }

struct Fvector2
{
    float x, y;
    Fvector2& set(float a, float b) { x = a; y = b; return *this; }
    Fvector2& sub(const Fvector2& o) { x -= o.x; y -= o.y; return *this; }
    Fvector2& add(const Fvector2& o) { x += o.x; y += o.y; return *this; }
    Fvector2& add(const Fvector2& a, const Fvector2& b) { x = a.x + b.x; y = a.y + b.y; return *this; }
    Fvector2& mul(float s) { x *= s; y *= s; return *this; }
};

struct Ivector2
{
    int x, y;
    Ivector2& set(int a, int b) { x = a; y = b; return *this; }
    Ivector2& add(const Ivector2& o) { x += o.x; y += o.y; return *this; }
    bool operator==(const Ivector2& o) const { return x == o.x && y == o.y; }
};

template <class T> struct RectVec;
template <> struct RectVec<int> { typedef Ivector2 type; };
template <> struct RectVec<float> { typedef Fvector2 type; };

// x1..y2 and lt/rb overlap the same storage, the way _rect does in the engine.
template <class T> struct Rect
{
    typedef typename RectVec<T>::type Tvector;
    union
    {
        struct { T x1, y1, x2, y2; };
        struct { Tvector lt, rb; };
    };
    Rect() { x1 = y1 = x2 = y2 = T(0); }
    Rect& set(T a, T b, T c, T d) { x1 = a; y1 = b; x2 = c; y2 = d; return *this; }
    Rect& set(const Rect& o) { x1 = o.x1; y1 = o.y1; x2 = o.x2; y2 = o.y2; return *this; }
    T width() const { return x2 - x1; }
    T height() const { return y2 - y1; }
    bool operator==(const Rect& o) const { return x1 == o.x1 && y1 == o.y1 && x2 == o.x2 && y2 == o.y2; }
    bool empty() const { return x2 < x1 || y2 < y1; }
};
typedef Rect<int> Irect;
typedef Rect<float> Frect;

static int iFloor(float f) { return (int)std::floor(f); }
template <class T> static T _min(T a, T b) { return a < b ? a : b; }
template <class T> static T _max(T a, T b) { return a > b ? a : b; }
template <class T> static void clamp(T& v, const T& lo, const T& hi) { if (v < lo) v = lo; else if (v > hi) v = hi; }
struct xrCriticalSectionGuard { explicit xrCriticalSectionGuard(int&) {} };
#define IC inline

enum EDropPreview { dpMerge, dpPlace, dpAuto };

struct SDropPrediction
{
    EDropPreview result;
    Irect attempted_cells;
    Irect final_cells;
};

// --- render capture -------------------------------------------------------------
struct Point { int batch; float x, y, z; u32 color; float u, v; };
struct IUIRender { enum ePrimitiveType { ptTriList }; enum ePointType { pttLIT }; };

struct RenderCapture
{
    std::vector<Point> points;
    int batch = -1;
    int shader_sets = 0;
    void StartPrimitive(u32, int, int) { ++batch; }
    void PushPoint(float x, float y, float z, u32 c, float u, float v) { points.push_back(Point{batch, x, y, z, c, u, v}); }
    void SetShader(int) { ++shader_sets; }
    void FlushPrimitive() {}
    void reset() { points.clear(); batch = -1; shader_sets = 0; }
    int batches() const { return batch + 1; }
    std::vector<Point> of(int b) const
    {
        std::vector<Point> res;
        for (const Point& p : points)
            if (p.batch == b)
                res.push_back(p);
        return res;
    }
};
static RenderCapture capture;
static RenderCapture* UIRender = &capture;

// ClientToScreenScaled is a plain per-axis scale; deliberately non-uniform here so an
// axis mix-up in the preview cannot pass, and deliberately fractional so the effective
// cell size is not the one from the XML.
static const float g_scale_x = 1.25f, g_scale_y = 1.4f;
struct UiCore
{
    int m_currentPointType = 0;
    Frect scissor;
    int scissor_depth = 0;
    float ClientToScreenScaledX(float v) const { return v * g_scale_x; }
    float ClientToScreenScaledY(float v) const { return v * g_scale_y; }
    void ClientToScreenScaled(Fvector2& dest, float left, float top) const { dest.set(left * g_scale_x, top * g_scale_y); }
    void PushScissor(const Frect& r) { scissor = r; ++scissor_depth; }
    void PopScissor() { --scissor_depth; }
};
static UiCore ui_core_instance;
static UiCore& UI() { return ui_core_instance; }
static struct { u32 dwFrame; } Device = { 7 };

struct ui_shader { int v = 0; int& operator*() { return v; } };

CONSTANTS

IC int screen_cell_len(int ui_len, float scale)
SCREEN_LEN_BODY

IC float snap_grid_px(float v)
SNAP_GRID_BODY

struct CUICellContainer;
struct CUICellItem;
struct CUICell;

struct CUIDragItem;

struct CUIDragDropListEx
{
    static CUIDragItem* m_drag_item;
    CUICellContainer* m_container = nullptr;
    u32 back_color = 0xFFFFFFFF;
    bool grouping = false, vertical = false, virtual_cells = false, custom_placement = true, auto_grow = false;
    int compactions = 0;
    int scroll_pos = 0;
    Frect client_area;

    bool IsGrouping() { return grouping; }
    bool GetVerticalPlacement() { return vertical; }
    bool GetVirtualCells() { return virtual_cells; }
    bool GetCustomPlacement() { return custom_placement; }
    bool IsAutoGrow() { return auto_grow; }
    int ScrollPos() { return scroll_pos; }
    void GetClientArea(Frect& r) { r = client_area; }
    void Compact() { ++compactions; }
    void Register(CUICellItem*) {}

    const Ivector2& CellsCapacity();
    virtual void SetItem(CUICellItem* itm);
    virtual bool SetItem(CUICellItem* itm, Fvector2 abs_pos);
    virtual void SetItem(CUICellItem* itm, Ivector2 cell_pos);
    virtual CUICellItem* RemoveItem(CUICellItem* itm, bool force_root);
    CUICell& GetCellAt(const Ivector2& pos);
    virtual SDropPrediction PredictDrop(CUICellItem* itm, const Fvector2& abs_pos, CUICellItem* skip = nullptr);
};

struct CUIWindow { virtual ~CUIWindow() {} };
typedef std::vector<CUIWindow*>::iterator WINDOW_LIST_it;

// Only the parts of a cell item the prediction and the drawing touch.
struct CUICellItem : CUIWindow
{
    Ivector2 grid;
    int kind;
    CUIDragDropListEx* owner = nullptr;
    bool m_cur_mark = false, m_selected = false, m_select_armament = false, m_select_equipped = false;
    u32 m_drawn_frame = 0;
    int drawn = 0;
    std::vector<CUICellItem*> children;
    CUICellItem(int w, int h, int k) : kind(k) { grid.set(w, h); }
    const Ivector2& GetGridSize() { return grid; }
    bool EqualTo(CUICellItem* o) { return kind != 0 && kind == o->kind; }
    CUIDragDropListEx* OwnerList() { return owner; }
    void SetOwnerList(CUIDragDropListEx* list) { owner = list; }
    void SetWindowName(const char*) {}
    void OnAfterChild(CUIDragDropListEx*) {}
    u32 ChildsCount() const { return u32(children.size()); }
    bool HasChild(CUICellItem* itm) const
    {
        return std::find(children.begin(), children.end(), itm) != children.end();
    }
    void PushChild(CUICellItem* itm) { children.push_back(itm); }
    CUICellItem* PopChild(CUICellItem* itm)
    {
        assert(!children.empty());
        std::vector<CUICellItem*>::iterator it = itm ? std::find(children.begin(), children.end(), itm) : children.end() - 1;
        assert(it != children.end());
        CUICellItem* result = *it;
        children.erase(it);
        return result;
    }
    void Draw() { ++drawn; }
};

struct CUIDragItem
{
    CUIDragDropListEx* back = nullptr;
    CUICellItem* parent = nullptr;
    Fvector2 pos;
    CUIDragDropListEx* BackList() { return back; }
    CUICellItem* ParentItem() { return parent; }
    Fvector2 GetPosition() { return pos; }
};
CUIDragItem* CUIDragDropListEx::m_drag_item = nullptr;

struct CUICell
{
    CUICellItem* m_item;
    CUICell() : m_item(nullptr) {}
    bool Empty() { return m_item == nullptr; }
    void SetItem(CUICellItem* item, bool) { m_item = item; }
    void Clear() { m_item = nullptr; }
    bool operator==(const CUICell& o) const { return m_item == o.m_item; }
};
typedef std::vector<CUICell> UI_CELLS_VEC;
typedef UI_CELLS_VEC::iterator UI_CELLS_VEC_IT;

struct CUICellContainer
{
    CUIDragDropListEx* m_pParentDragDropList = nullptr;
    int m_screenCellSize = 0;
    Ivector2 m_cellsCapacity{0, 0};
    Ivector2 m_cellSizeRaw{0, 0}, m_cellSpacingRaw{0, 0};
    Ivector2 m_cellSizeScreen{0, 0}, m_cellSpacingScreen{0, 0};
    Fvector2 m_cellSize{0.0f, 0.0f}, m_cellSpacing{0.0f, 0.0f}, m_metricsScale{1.0f, 1.0f};
    Fvector2 origin;
    bool m_isInventoryGridDisabled = false;
    ui_shader hShader;
    UI_CELLS_VEC m_cells, m_cells_to_draw;
    std::vector<CUIWindow*> m_ChildWndList;
    int csUi = 0;

    void GetAbsolutePos(Fvector2& p) { p = origin; }
    const Fvector2& CellSize() { return m_cellSize; }
    const Fvector2& CellsSpacing() { return m_cellSpacing; }

    bool UpdateCellMetrics() METRICS_BODY
    Fvector2 CellOffsetUI(const Ivector2& cell_pos) const OFFSET_BODY

    bool ValidCell(const Ivector2& pos) const VALID_BODY
    CUICell& GetCellAt(const Ivector2& pos) CELL_AT_BODY
    Ivector2 PickCell(const Fvector2& abs_pos) PICK_BODY
    bool IsRoomFree(const Ivector2& pos, const Ivector2& _size, const Ivector2& capacity, const CUICellItem* ignore) ROOM_CAPACITY_BODY
    bool IsRoomFree(const Ivector2& pos, const Ivector2& _size, const CUICellItem* ignore = nullptr) ROOM_BODY
    bool FindFreeCellInCapacity(const Ivector2& _size, const Ivector2& capacity, Ivector2& out_pos, const CUICellItem* ignore = nullptr) FIND_CAPACITY_BODY
    bool ResolveFreeCell(const Ivector2& _size, Ivector2& out_pos, Ivector2& out_capacity, const CUICellItem* ignore = nullptr) RESOLVE_BODY
    Ivector2 FindFreeCell(const Ivector2& _size) FIND_BODY
    bool AddSimilar(CUICellItem* itm) BODY_ADD_SIMILAR
    CUICellItem* FindSimilar(CUICellItem* itm, CUICellItem* skip = nullptr) SIMILAR_BODY
    Ivector2 GetItemPos(CUICellItem* itm) ITEM_POS_BODY
    void PlaceItemAtPos(CUICellItem* itm, Ivector2& cell_pos) BODY_PLACE_ITEM
    CUICellItem* RemoveItem(CUICellItem* itm, bool force_root) BODY_CONTAINER_REMOVE
    void SetItemGeometry(CUICellItem*, const Ivector2&) {}
    void AttachChild(CUICellItem* itm) { m_ChildWndList.push_back(itm); }
    void DetachChild(CUICellItem* itm)
    {
        m_ChildWndList.erase(std::remove(m_ChildWndList.begin(), m_ChildWndList.end(), itm), m_ChildWndList.end());
    }
    void GetTexUVLT(Fvector2& uv, u32 col, u32 row, u8 select_mode) TEX_UV_BODY
    Ivector2 TopVisibleCell() TOP_CELL_BODY
    u32 GetCellsInRange(const Irect& rect, UI_CELLS_VEC& res) IN_RANGE_BODY
    void Draw() DRAW_BODY
    void DrawDropPreview(const Irect& tgt_cells, const Fvector2& draw_lt, const Fvector2& f_len, const Fvector2& sp_len) PREVIEW_BODY
    void Grow() GROW_BODY

    // Test scaffolding, not production code.
    void reset(int cols, int rows)
    {
        m_cellsCapacity.set(cols, rows);
        m_cells.assign(size_t(cols * rows), CUICell());
        m_ChildWndList.clear();
    }
    void SetCellsCapacity(const Ivector2& capacity)
    {
        const Ivector2 old_capacity = m_cellsCapacity;
        const UI_CELLS_VEC old_cells = m_cells;
        m_cellsCapacity = capacity;
        m_cells.assign(size_t(capacity.x * capacity.y), CUICell());
        for (int y = 0; y < std::min(old_capacity.y, capacity.y); ++y)
            for (int x = 0; x < std::min(old_capacity.x, capacity.x); ++x)
                m_cells[size_t(capacity.x * y + x)] = old_cells[size_t(old_capacity.x * y + x)];
    }
    void put(CUICellItem* itm, int cx, int cy)
    {
        Ivector2 size = itm->grid;
        if (m_pParentDragDropList->GetVerticalPlacement())
            std::swap(size.x, size.y);
        for (int x = 0; x < size.x; ++x)
            for (int y = 0; y < size.y; ++y)
            {
                Ivector2 p;
                p.set(cx + x, cy + y);
                GetCellAt(p).m_item = itm;
            }
        itm->owner = m_pParentDragDropList;
        m_ChildWndList.push_back(itm);
    }
    Fvector2 aim(int cx, int cy) const
    {
        Ivector2 c;
        c.set(cx, cy);
        Fvector2 p;
        p.add(origin, CellOffsetUI(c));
        p.x += m_cellSize.x * 0.5f;
        p.y += m_cellSize.y * 0.5f;
        return p;
    }
};

const Ivector2& CUIDragDropListEx::CellsCapacity() { return m_container->m_cellsCapacity; }
CUICell& CUIDragDropListEx::GetCellAt(const Ivector2& pos) { return m_container->GetCellAt(pos); }

SDropPrediction CUIDragDropListEx::PredictDrop(CUICellItem* itm, const Fvector2& abs_pos, CUICellItem* skip)
PREDICT_BODY

void CUIDragDropListEx::SetItem(CUICellItem* itm)
BODY_LIST_SET_AUTO

bool CUIDragDropListEx::SetItem(CUICellItem* itm, Fvector2 abs_pos)
BODY_LIST_SET_ABS

void CUIDragDropListEx::SetItem(CUICellItem* itm, Ivector2 cell_pos)
BODY_LIST_SET_CELL

CUICellItem* CUIDragDropListEx::RemoveItem(CUICellItem* itm, bool force_root)
BODY_LIST_REMOVE

struct CUIDragDropReferenceList : CUIDragDropListEx
{
    bool SetItem(CUICellItem* itm, Fvector2 abs_pos) override;
    void SetItem(CUICellItem* itm, Ivector2 cell_pos) override { CUIDragDropListEx::SetItem(itm, cell_pos); }
    SDropPrediction PredictDrop(CUICellItem* itm, const Fvector2& abs_pos, CUICellItem* skip = nullptr) override;
};

SDropPrediction CUIDragDropReferenceList::PredictDrop(CUICellItem* itm, const Fvector2& abs_pos, CUICellItem* /*skip*/)
REF_DROP_BODY

bool CUIDragDropReferenceList::SetItem(CUICellItem* itm, Fvector2 abs_pos)
REF_SET_ABS_BODY

struct ForcedDropList : CUIDragDropListEx
{
    SDropPrediction forced;
    SDropPrediction PredictDrop(CUICellItem*, const Fvector2&, CUICellItem* = nullptr) override { return forced; }
};

// --- helpers --------------------------------------------------------------------

// The grid quads carry a per cell UV fingerprint once the grid texture is in its
// "disabled" mode, so a quad can be traced back to the cell it was drawn for.
static std::vector<Point> quad_for(const std::vector<Point>& pts, const Fvector2& tp)
{
    for (size_t i = 0; i + 6 <= pts.size(); i += 6)
        if (std::fabs(pts[i].u - tp.x) < 1e-6f && std::fabs(pts[i].v - tp.y) < 1e-6f)
            return std::vector<Point>(pts.begin() + long(i), pts.begin() + long(i) + 6);
    return std::vector<Point>();
}

static void same_geometry(const std::vector<Point>& a, const std::vector<Point>& b)
{
    assert(a.size() == 6 && b.size() == 6);
    for (int k = 0; k < 6; ++k)
    {
        assert(a[size_t(k)].x == b[size_t(k)].x);
        assert(a[size_t(k)].y == b[size_t(k)].y);
    }
}

static void assert_footprint(CUICellContainer& box, CUICellItem* itm, const Irect& footprint)
{
    int occupied = 0;
    for (int y = 0; y < box.m_cellsCapacity.y; ++y)
        for (int x = 0; x < box.m_cellsCapacity.x; ++x)
        {
            Ivector2 pos;
            pos.set(x, y);
            if (box.GetCellAt(pos).m_item != itm)
                continue;

            assert(x >= footprint.x1 && x <= footprint.x2);
            assert(y >= footprint.y1 && y <= footprint.y2);
            ++occupied;
        }

    assert(occupied == (footprint.x2 - footprint.x1 + 1) * (footprint.y2 - footprint.y1 + 1));
}

struct Scene
{
    CUIDragDropListEx list;
    CUICellContainer box;
    CUIDragItem drag;

    Scene(int cols, int rows, int cell, int space)
    {
        list.m_container = &box;
        box.m_pParentDragDropList = &list;
        box.m_isInventoryGridDisabled = true;   // unique UV per cell
        box.m_cellSizeRaw.set(cell, cell);
        box.m_cellSpacingRaw.set(space, 0);
        box.UpdateCellMetrics();
        box.origin.set(702.0f, 119.0f);
        box.reset(cols, rows);
        list.client_area.set(702.0f, 119.0f,
                             702.0f + (box.m_cellSize.x + box.m_cellSpacing.x) * float(cols),
                             119.0f + (box.m_cellSize.y + box.m_cellSpacing.y) * float(rows));
        drag.back = &list;
        CUIDragDropListEx::m_drag_item = nullptr;
    }

    // Draw once with no drag (grid only) and once with the item hovering, then return
    // the grid batch and the preview batch of the second pass.
    void run(CUICellItem* itm, int cx, int cy)
    {
        drag.parent = itm;
        drag.pos = box.aim(cx, cy);
        CUIDragDropListEx::m_drag_item = &drag;
        capture.reset();
        box.Draw();
    }
};

int main()
{
    // ---- prediction --------------------------------------------------------------
    // gamedata/configs/ui/actor_menu.xml, dragdrop_bag: 7x14 cells of 41x41.
    Scene bag(7, 14, 41, 0);
    CUIDragDropListEx& list = bag.list;
    CUICellContainer& box = bag.box;

    CUICellItem medkit(1, 1, 1);
    Irect want;
    SDropPrediction prediction;

    // Empty cell: the item goes exactly where the cursor points.
    prediction = list.PredictDrop(&medkit, box.aim(3, 5));
    assert(prediction.result == dpPlace);
    assert(prediction.attempted_cells == want.set(3, 5, 3, 5));
    assert(prediction.final_cells == prediction.attempted_cells);

    // Occupied cell: automatic placement takes over, but the cells the cursor points
    // at are still reported so the preview can mark them as blocked.
    CUICellItem bread(1, 1, 2);
    box.put(&bread, 3, 5);
    prediction = list.PredictDrop(&medkit, box.aim(3, 5));
    assert(prediction.result == dpAuto);
    assert(prediction.attempted_cells == want.set(3, 5, 3, 5));
    assert(prediction.final_cells == want.set(0, 0, 0, 0));

    // The prediction itself is pure: it has neither grown nor repacked the list.
    assert((box.m_cellsCapacity == Ivector2{7, 14}));
    assert(box.GetCellAt(Ivector2{3, 5}).m_item == &bread);
    assert(list.compactions == 0);

    // The production SetItem(abs_pos) dispatches dpAuto through automatic placement
    // and fills exactly the complete footprint predicted above.
    list.SetItem(&medkit, box.aim(3, 5));
    assert(box.GetItemPos(&medkit) == prediction.final_cells.lt);
    assert_footprint(box, &medkit, prediction.final_cells);
    assert(list.RemoveItem(&medkit, true) == &medkit);

    // Outside the grid: nothing to point at.
    Fvector2 above;
    above.set(box.origin.x + 20.0f, box.origin.y - 20.0f);
    prediction = list.PredictDrop(&medkit, above);
    assert(prediction.result == dpAuto);
    assert(prediction.attempted_cells.empty());
    assert(prediction.final_cells == want.set(0, 0, 0, 0));

    // A 2x1 weapon nudged one cell to the right overlaps the cell it is leaving. The
    // real drop calls RemoveItem first, so the preview has to ignore the dragged item
    // - otherwise moving a weapon by one cell would always look blocked.
    box.reset(7, 14);
    CUICellItem rifle(2, 1, 3);
    box.put(&rifle, 0, 0);
    prediction = list.PredictDrop(&rifle, box.aim(1, 0), &rifle);
    assert(prediction.result == dpPlace);
    assert(prediction.attempted_cells == want.set(1, 0, 2, 0));
    assert(prediction.final_cells == prediction.attempted_cells);
    assert(list.PredictDrop(&rifle, box.aim(1, 0), nullptr).result == dpAuto);

    // The footprint is the whole item, not just the cell under the cursor.
    box.reset(7, 14);
    CUICellItem suit(2, 3, 4);
    prediction = list.PredictDrop(&suit, box.aim(1, 2));
    assert(prediction.result == dpPlace);
    assert(prediction.attempted_cells == want.set(1, 2, 2, 4));
    assert(prediction.final_cells == prediction.attempted_cells);

    // A footprint running off the right edge cannot be placed.
    prediction = list.PredictDrop(&suit, box.aim(6, 2));
    assert(prediction.result == dpAuto);
    assert(prediction.attempted_cells == want.set(6, 2, 7, 4));
    assert(prediction.final_cells == want.set(0, 0, 1, 2));

    // Vertical lists swap the footprint, the way PlaceItemAtPos does.
    list.vertical = true;
    box.reset(7, 14);
    prediction = list.PredictDrop(&suit, box.aim(1, 2));
    assert(prediction.result == dpPlace);
    assert(prediction.attempted_cells == want.set(1, 2, 3, 3));
    assert(prediction.final_cells == prediction.attempted_cells);
    list.vertical = false;

    // Grouping wins over position: the item merges into the stack it matches, wherever
    // the cursor is, and the preview points at that stack.
    box.reset(7, 14);
    list.grouping = true;
    CUICellItem stack(1, 1, 1);
    box.put(&stack, 2, 3);
    prediction = list.PredictDrop(&medkit, box.aim(6, 12));
    assert(prediction.result == dpMerge);
    assert(prediction.attempted_cells == want.set(2, 3, 2, 3));
    assert(prediction.final_cells == prediction.attempted_cells);

    // A different item still lands under the cursor.
    CUICellItem bolt(1, 1, 9);
    prediction = list.PredictDrop(&bolt, box.aim(6, 12));
    assert(prediction.result == dpPlace);
    assert(prediction.attempted_cells == want.set(6, 12, 6, 12));

    // Dragging the stack itself inside its own list must not match itself.
    prediction = list.PredictDrop(&stack, box.aim(6, 12), &stack);
    assert(prediction.result == dpPlace);
    assert(prediction.attempted_cells == want.set(6, 12, 6, 12));

    // Two identical stacks: the one that is not being dragged is the merge target.
    CUICellItem other(1, 1, 1);
    box.put(&other, 4, 4);
    prediction = list.PredictDrop(&stack, box.aim(6, 12), &stack);
    assert(prediction.result == dpMerge);
    assert(prediction.attempted_cells == want.set(4, 4, 4, 4));
    assert(prediction.final_cells == prediction.attempted_cells);

    CUICellItem merge_item(1, 1, 1);
    const u32 stack_children = stack.ChildsCount();
    list.SetItem(&merge_item, box.aim(6, 12));
    assert(stack.ChildsCount() == stack_children + 1);
    assert(stack.HasChild(&merge_item));
    assert(merge_item.OwnerList() == &list);
    list.grouping = false;

    // A same-list auto placement sees the exact state after RemoveItem: the dragged
    // weapon's old cells are the first fit, without changing the live grid in preview.
    box.reset(3, 1);
    CUICellItem same_list_rifle(2, 1, 10);
    CUICellItem end_blocker(1, 1, 11);
    box.put(&same_list_rifle, 0, 0);
    box.put(&end_blocker, 2, 0);
    prediction = list.PredictDrop(&same_list_rifle, box.aim(2, 0), &same_list_rifle);
    assert(prediction.result == dpAuto);
    assert(prediction.attempted_cells == want.set(2, 0, 3, 0));
    assert(prediction.final_cells == want.set(0, 0, 1, 0));
    assert(box.GetCellAt(Ivector2{0, 0}).m_item == &same_list_rifle);
    CUICellItem* moved_rifle = list.RemoveItem(&same_list_rifle, true);
    assert(moved_rifle == &same_list_rifle);
    list.SetItem(moved_rifle, box.aim(2, 0));
    assert(box.GetItemPos(moved_rifle) == prediction.final_cells.lt);
    assert_footprint(box, moved_rifle, prediction.final_cells);

    // A full fixed grid has no trustworthy final target without entering the legacy
    // Compact fallback. Prediction stays side-effect free and reports no final cells.
    box.reset(2, 1);
    CUICellItem fixed_full(2, 1, 19);
    box.put(&fixed_full, 0, 0);
    prediction = list.PredictDrop(&medkit, box.aim(0, 0));
    assert(prediction.result == dpAuto);
    assert(prediction.attempted_cells == want.set(0, 0, 0, 0));
    assert(prediction.final_cells.empty());
    assert(list.compactions == 0);

    // Auto-grow is predicted without mutating capacity, then production SetItem
    // performs exactly the required growth and fills the predicted footprint.
    box.reset(2, 1);
    list.auto_grow = true;
    CUICellItem full_row(2, 1, 12);
    CUICellItem crate(2, 2, 13);
    box.put(&full_row, 0, 0);
    prediction = list.PredictDrop(&crate, box.aim(0, 0));
    assert(prediction.result == dpAuto);
    assert(prediction.final_cells == want.set(0, 1, 1, 2));
    assert((box.m_cellsCapacity == Ivector2{2, 1}));
    list.SetItem(&crate, box.aim(0, 0));
    assert(box.GetItemPos(&crate) == prediction.final_cells.lt);
    assert_footprint(box, &crate, prediction.final_cells);
    assert((box.m_cellsCapacity == Ivector2{2, 3}));

    // Vertical placement keeps the complete swapped footprint through growth.
    box.reset(3, 1);
    list.vertical = true;
    CUICellItem vertical_row(1, 3, 14);
    CUICellItem vertical_item(2, 3, 15);
    box.put(&vertical_row, 0, 0);
    prediction = list.PredictDrop(&vertical_item, box.aim(0, 0));
    assert(prediction.result == dpAuto);
    assert(prediction.final_cells == want.set(0, 1, 2, 2));
    assert((box.m_cellsCapacity == Ivector2{3, 1}));
    list.SetItem(&vertical_item, box.aim(0, 0));
    assert(box.GetItemPos(&vertical_item) == prediction.final_cells.lt);
    assert_footprint(box, &vertical_item, prediction.final_cells);
    assert((box.m_cellsCapacity == Ivector2{3, 3}));
    list.vertical = false;
    list.auto_grow = false;

    // Quick slots replace an occupied reference cell and never acquire a second
    // automatic-placement target.
    {
        CUIDragDropReferenceList refs;
        CUICellContainer ref_box;
        refs.m_container = &ref_box;
        ref_box.m_pParentDragDropList = &refs;
        ref_box.origin.set(0.0f, 0.0f);
        ref_box.m_cellSizeRaw.set(41, 41);
        ref_box.m_cellSpacingRaw.set(0, 0);
        ref_box.UpdateCellMetrics();
        ref_box.reset(4, 1);
        CUICellItem old_ref(1, 1, 16);
        CUICellItem new_ref(1, 1, 17);
        ref_box.put(&old_ref, 2, 0);
        prediction = refs.PredictDrop(&new_ref, ref_box.aim(2, 0));
        assert(prediction.result == dpPlace);
        assert(prediction.attempted_cells == want.set(2, 0, 2, 0));
        assert(prediction.final_cells == prediction.attempted_cells);
        assert(refs.SetItem(&new_ref, ref_box.aim(2, 0)));
        assert(old_ref.OwnerList() == nullptr);
        assert(ref_box.GetItemPos(&new_ref) == prediction.final_cells.lt);
        assert_footprint(ref_box, &new_ref, prediction.final_cells);

        CUICellItem oversized(5, 1, 18);
        prediction = refs.PredictDrop(&oversized, ref_box.aim(2, 0));
        assert(prediction.result == dpAuto);
        assert(prediction.attempted_cells.empty());
        assert(prediction.final_cells.empty());
    }

    // ---- the highlight lands on the cell it claims --------------------------------
    // The preview is drawn from its own copy of the cell geometry; these checks pin it
    // to the pixels the grid pass itself uses for the same cell.
    {
        Scene s(7, 14, 41, 0);
        CUICellItem pill(1, 1, 5);
        s.run(&pill, 4, 6);

        assert(capture.batches() == 2);
        Fvector2 grid_uv, preview_uv;
        s.box.GetTexUVLT(grid_uv, 4, 6, 0);
        s.box.GetTexUVLT(preview_uv, 4, 6, 1);
        const std::vector<Point> grid_quad = quad_for(capture.of(0), grid_uv);
        const std::vector<Point> preview = quad_for(capture.of(1), preview_uv);
        assert(!grid_quad.empty());
        same_geometry(grid_quad, preview);
        assert(grid_uv.x < 0.25f && preview_uv.x >= 0.25f && preview_uv.x < 0.5f);
        assert(preview[0].color == subst_alpha(kDropPreviewFree, 240));
    }

    // With the ordinary grid texture its normal slice is visible, so preview keeps
    // using slice zero and the original alpha.
    {
        Scene s(7, 14, 41, 0);
        s.box.m_isInventoryGridDisabled = false;
        CUICellItem pill(1, 1, 5);
        s.run(&pill, 4, 6);

        assert(capture.batches() == 2);
        const std::vector<Point> preview = capture.of(1);
        assert(preview.size() == 6);
        assert(preview[0].u == 0.0f && preview[0].v == 0.0f);
        assert(preview[0].color == kDropPreviewFree);
    }

    // Scrolled list: the preview indexes cells absolutely while the grid pass counts
    // from the first visible row, so this is where an off by one would show up.
    {
        Scene s(7, 14, 41, 0);
        // Three whole rows down, in UI base units the scroll bar works in.
        s.list.scroll_pos = iFloor(3.0f * (s.box.m_cellSize.y + s.box.m_cellSpacing.y)) + 1;
        CUICellItem pill(1, 1, 5);
        s.run(&pill, 4, 6);

        assert(s.box.TopVisibleCell().y == 3);
        assert(capture.batches() == 2);
        Fvector2 grid_uv, preview_uv;
        s.box.GetTexUVLT(grid_uv, 4, 6, 0);
        s.box.GetTexUVLT(preview_uv, 4, 6, 1);
        same_geometry(quad_for(capture.of(0), grid_uv), quad_for(capture.of(1), preview_uv));
    }

    // Spaced list, like the belt: cell pitch is not the cell size.
    {
        Scene s(5, 1, 41, 24);
        CUICellItem pill(1, 1, 5);
        const Ivector2 cell = s.box.PickCell(s.box.aim(3, 0));
        s.run(&pill, 3, 0);

        assert(capture.batches() == 2);
        Fvector2 grid_uv, preview_uv;
        s.box.GetTexUVLT(grid_uv, u32(cell.x), u32(cell.y), 0);
        s.box.GetTexUVLT(preview_uv, u32(cell.x), u32(cell.y), 1);
        same_geometry(quad_for(capture.of(0), grid_uv), quad_for(capture.of(1), preview_uv));
    }

    // A 2x1 item highlights both of its cells, each on top of its own grid cell.
    {
        Scene s(7, 14, 41, 0);
        CUICellItem gun(2, 1, 6);
        s.run(&gun, 2, 8);

        assert(capture.batches() == 2);
        const std::vector<Point> preview = capture.of(1);
        assert(preview.size() == 12);
        for (int i = 0; i < 2; ++i)
        {
            Fvector2 grid_uv, preview_uv;
            s.box.GetTexUVLT(grid_uv, u32(2 + i), 8, 0);
            s.box.GetTexUVLT(preview_uv, u32(2 + i), 8, 1);
            same_geometry(quad_for(capture.of(0), grid_uv), quad_for(preview, preview_uv));
        }
    }

    // Blocked cells get the other tint.
    {
        Scene s(7, 14, 41, 0);
        CUICellItem pill(1, 1, 5);
        CUICellItem taken(1, 1, 7);
        s.box.put(&taken, 4, 6);
        s.run(&pill, 4, 6);

        assert(capture.batches() == 3);
        assert(capture.of(1)[0].color == subst_alpha(kDropPreviewBlocked, 240));
        assert(capture.of(2)[0].color == subst_alpha(kDropPreviewFree, 240));
        Fvector2 attempted_grid_uv, attempted_preview_uv, final_grid_uv, final_preview_uv;
        s.box.GetTexUVLT(attempted_grid_uv, 4, 6, 0);
        s.box.GetTexUVLT(attempted_preview_uv, 4, 6, 1);
        s.box.GetTexUVLT(final_grid_uv, 0, 0, 0);
        s.box.GetTexUVLT(final_preview_uv, 0, 0, 1);
        same_geometry(quad_for(capture.of(0), attempted_grid_uv), quad_for(capture.of(1), attempted_preview_uv));
        same_geometry(quad_for(capture.of(0), final_grid_uv), quad_for(capture.of(2), final_preview_uv));
        assert(kDropPreviewBlocked != kDropPreviewFree);
        // Drawn after the item, so the tint is not hidden under the icon.
        assert(taken.drawn == 1);
    }

    // Each footprint is clipped independently. A final target outside the current
    // viewport adds no stray quad; after scrolling to it the green cells are correct.
    {
        Scene s(4, 8, 41, 0);
        s.list.client_area.y2 = s.list.client_area.y1 + 3.0f * s.box.m_cellSize.y;
        std::vector<CUICellItem> blockers;
        blockers.reserve(5);
        for (int row = 0; row < 5; ++row)
        {
            blockers.emplace_back(4, 1, 30 + row);
            s.box.put(&blockers.back(), 0, row);
        }
        CUICellItem pill(1, 1, 5);
        s.run(&pill, 1, 1);
        assert(capture.batches() == 2);
        assert(capture.of(1)[0].color == subst_alpha(kDropPreviewBlocked, 240));

        s.list.scroll_pos = iFloor(5.0f * s.box.m_cellSize.y) + 1;
        s.run(&pill, 1, 1);
        assert(s.box.TopVisibleCell().y == 5);
        assert(capture.batches() == 2);
        assert(capture.of(1)[0].color == subst_alpha(kDropPreviewFree, 240));
        Fvector2 final_grid_uv, final_preview_uv;
        s.box.GetTexUVLT(final_grid_uv, 0, 5, 0);
        s.box.GetTexUVLT(final_preview_uv, 0, 5, 1);
        same_geometry(quad_for(capture.of(0), final_grid_uv), quad_for(capture.of(1), final_preview_uv));
    }

    // If an unusual override resolves dpAuto to the attempted rectangle itself,
    // retain the existing blocked signal and do not draw a duplicate batch.
    {
        ForcedDropList forced_list;
        CUICellContainer forced_box;
        forced_list.m_container = &forced_box;
        forced_box.m_pParentDragDropList = &forced_list;
        forced_box.m_isInventoryGridDisabled = true;
        forced_box.m_cellSizeRaw.set(41, 41);
        forced_box.m_cellSpacingRaw.set(0, 0);
        forced_box.UpdateCellMetrics();
        forced_box.origin.set(0.0f, 0.0f);
        forced_box.reset(3, 3);
        forced_list.client_area.set(0.0f, 0.0f, 3.0f * forced_box.m_cellSize.x, 3.0f * forced_box.m_cellSize.y);
        forced_list.forced.result = dpAuto;
        forced_list.forced.attempted_cells.set(1, 1, 1, 1);
        forced_list.forced.final_cells.set(1, 1, 1, 1);

        CUICellItem pill(1, 1, 5);
        CUIDragItem drag;
        drag.back = &forced_list;
        drag.parent = &pill;
        drag.pos = forced_box.aim(1, 1);
        CUIDragDropListEx::m_drag_item = &drag;
        capture.reset();
        forced_box.Draw();
        assert(capture.batches() == 2);
        assert(capture.of(1)[0].color == subst_alpha(kDropPreviewBlocked, 240));
    }

    // ---- lists that must not be highlighted ---------------------------------------
    {
        Scene s(7, 14, 41, 0);
        CUICellItem pill(1, 1, 5);

        capture.reset();
        s.box.Draw();
        assert(capture.batches() == 1);          // no drag at all

        s.list.virtual_cells = true;
        s.run(&pill, 4, 6);
        assert(capture.batches() == 1);          // equipment slots center the item
        s.list.virtual_cells = false;

        s.drag.back = nullptr;
        capture.reset();
        s.box.Draw();
        assert(capture.batches() == 1);          // cursor is over another list
        s.drag.back = &s.list;

        s.box.put(&pill, 0, 0);                  // now owned by this list
        s.list.custom_placement = false;
        s.run(&pill, 4, 6);
        assert(capture.batches() == 1);          // OnItemDrop ignores this move
        s.list.custom_placement = true;
    }
    {
        Scene trash(1, 1, 340, 0);               // dragdrop_trash: one huge cell
        CUICellItem pill(1, 1, 5);
        trash.run(&pill, 0, 0);
        assert(capture.batches() == 1);
    }

    std::printf("ok\n");
    return 0;
}
'''

code = code.replace('CONSTANTS', constants)
for token, text in bodies.items():
    code = code.replace(token, text)

with tempfile.TemporaryDirectory(prefix='ixray-drop-preview-') as directory:
    source = Path(directory) / 'test.cpp'
    source.write_text(code)
    binary = source.with_suffix('')
    subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    # GetItemPos is extracted verbatim and carries the upstream indentation.
                    '-Wno-misleading-indentation',
                    '-fsanitize=address,undefined', '-g', str(source), '-o', str(binary)], check=True)
    # LeakSanitizer cannot run inside the desktop sandbox; retain address/UB checks.
    subprocess.run([str(binary)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS=os.environ.get('ASAN_OPTIONS', 'detect_leaks=0')))

print('PASS: production PredictDrop, ResolveFreeCell/FindFreeCell, SetItem/RemoveItem/PlaceItemAtPos, '
      'reference-list PredictDrop/SetItem and production Draw/DrawDropPreview; attempted/final '
      'footprints for free, blocked and off-grid targets; real same-list remove/drop, multi-cell, '
      'auto-grow and vertical auto-grow placement; grouping and quick-slot replacement; red attempted plus '
      'green final rendering, independent scroll clipping and duplicate suppression; highlight pixels '
      'matched for plain, scrolled, spaced and 2x1 cases; visible compensated ui_grid_alt mask and ordinary '
      'grid UVs; no highlight for virtual cells, foreign '
      'list, fixed placement or single-cell list; ASan/UBSan')
