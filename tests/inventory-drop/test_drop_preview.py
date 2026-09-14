"""Compile the production drop prediction and the production cell drawing, then check
that the cells highlighted while dragging are the cells SetItem would fill, and that
the highlight is painted on exactly the same pixels as the grid cell underneath."""
from pathlib import Path
import os
import re
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

# The cell tints and the UV span live in the anonymous namespace at the top of the file.
constants = '\n'.join(re.findall(r'^constexpr (?:float|u32) k\w+\s*=.*?;$', drag_drop, re.M))
assert 'kDropPreviewFree' in constants and 'kInventoryCellUSpanGridDisabled' in constants, constants
assert 'kCellPixelBias' in constants, constants

bodies = {
    'PREDICT_BODY': body(drag_drop, 'EDropPreview CUIDragDropListEx::PredictDrop('),
    'PICK_BODY': body(drag_drop, 'Ivector2 CUICellContainer::PickCell('),
    'VALID_BODY': body(drag_drop, 'bool CUICellContainer::ValidCell('),
    'ROOM_BODY': body(drag_drop, 'bool CUICellContainer::IsRoomFree('),
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
}

code = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include <utility>
#include <algorithm>

#define R_ASSERT(x) assert(x)
typedef unsigned int u32;
typedef unsigned char u8;
static const float EPS = 0.0000001f;
constexpr u32 color_rgba(u32 r, u32 g, u32 b, u32 a) { return ((a & 0xffu) << 24) | ((b & 0xffu) << 16) | ((g & 0xffu) << 8) | (r & 0xffu); }

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

struct CUICellContainer;
struct CUICellItem;

struct CUIDragItem;

struct CUIDragDropListEx
{
    static CUIDragItem* m_drag_item;
    CUICellContainer* m_container = nullptr;
    u32 back_color = 0xFFFFFFFF;
    bool grouping = false, vertical = false, virtual_cells = false, custom_placement = true;
    int scroll_pos = 0;
    Frect client_area;

    bool IsGrouping() { return grouping; }
    bool GetVerticalPlacement() { return vertical; }
    bool GetVirtualCells() { return virtual_cells; }
    bool GetCustomPlacement() { return custom_placement; }
    int ScrollPos() { return scroll_pos; }
    void GetClientArea(Frect& r) { r = client_area; }

    const Ivector2& CellsCapacity();
    EDropPreview PredictDrop(CUICellItem* itm, const Fvector2& abs_pos, Irect& out_cells, CUICellItem* skip = nullptr);
};

struct CUIWindow { virtual ~CUIWindow() {} };

// Only the parts of a cell item the prediction and the drawing touch.
struct CUICellItem : CUIWindow
{
    Ivector2 grid;
    int kind;
    CUIDragDropListEx* owner = nullptr;
    bool m_cur_mark = false, m_selected = false, m_select_armament = false, m_select_equipped = false;
    u32 m_drawn_frame = 0;
    int drawn = 0;
    CUICellItem(int w, int h, int k) : kind(k) { grid.set(w, h); }
    const Ivector2& GetGridSize() { return grid; }
    bool EqualTo(CUICellItem* o) { return kind != 0 && kind == o->kind; }
    CUIDragDropListEx* OwnerList() { return owner; }
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
    bool operator==(const CUICell& o) const { return m_item == o.m_item; }
};
typedef std::vector<CUICell> UI_CELLS_VEC;
typedef UI_CELLS_VEC::iterator UI_CELLS_VEC_IT;

struct CUICellContainer
{
    CUIDragDropListEx* m_pParentDragDropList = nullptr;
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
    bool IsRoomFree(const Ivector2& pos, const Ivector2& _size, const CUICellItem* ignore = nullptr) ROOM_BODY
    CUICellItem* FindSimilar(CUICellItem* itm, CUICellItem* skip = nullptr) SIMILAR_BODY
    Ivector2 GetItemPos(CUICellItem* itm) ITEM_POS_BODY
    void GetTexUVLT(Fvector2& uv, u32 col, u32 row, u8 select_mode) TEX_UV_BODY
    Ivector2 TopVisibleCell() TOP_CELL_BODY
    u32 GetCellsInRange(const Irect& rect, UI_CELLS_VEC& res) IN_RANGE_BODY
    void Draw() DRAW_BODY
    void DrawDropPreview(const Irect& tgt_cells, const Fvector2& draw_lt, const Fvector2& f_len, const Fvector2& sp_len) PREVIEW_BODY

    // Test scaffolding, not production code.
    void reset(int cols, int rows)
    {
        m_cellsCapacity.set(cols, rows);
        m_cells.assign(size_t(cols * rows), CUICell());
        m_ChildWndList.clear();
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

EDropPreview CUIDragDropListEx::PredictDrop(CUICellItem* itm, const Fvector2& abs_pos, Irect& out_cells, CUICellItem* skip)
PREDICT_BODY

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

static void same_pixels(const std::vector<Point>& a, const std::vector<Point>& b)
{
    assert(a.size() == 6 && b.size() == 6);
    for (int k = 0; k < 6; ++k)
    {
        assert(a[size_t(k)].x == b[size_t(k)].x);
        assert(a[size_t(k)].y == b[size_t(k)].y);
        assert(a[size_t(k)].u == b[size_t(k)].u);
        assert(a[size_t(k)].v == b[size_t(k)].v);
    }
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
    Irect cells;
    Irect want;

    // Empty cell: the item goes exactly where the cursor points.
    assert(list.PredictDrop(&medkit, box.aim(3, 5), cells) == dpPlace);
    assert(cells == want.set(3, 5, 3, 5));

    // Occupied cell: automatic placement takes over, but the cells the cursor points
    // at are still reported so the preview can mark them as blocked.
    CUICellItem bread(1, 1, 2);
    box.put(&bread, 3, 5);
    assert(list.PredictDrop(&medkit, box.aim(3, 5), cells) == dpAuto);
    assert(cells == want.set(3, 5, 3, 5));

    // Outside the grid: nothing to point at.
    Fvector2 above;
    above.set(box.origin.x + 20.0f, box.origin.y - 20.0f);
    assert(list.PredictDrop(&medkit, above, cells) == dpAuto);
    assert(cells.empty());

    // A 2x1 weapon nudged one cell to the right overlaps the cell it is leaving. The
    // real drop calls RemoveItem first, so the preview has to ignore the dragged item
    // - otherwise moving a weapon by one cell would always look blocked.
    box.reset(7, 14);
    CUICellItem rifle(2, 1, 3);
    box.put(&rifle, 0, 0);
    assert(list.PredictDrop(&rifle, box.aim(1, 0), cells, &rifle) == dpPlace);
    assert(cells == want.set(1, 0, 2, 0));
    assert(list.PredictDrop(&rifle, box.aim(1, 0), cells, nullptr) == dpAuto);

    // The footprint is the whole item, not just the cell under the cursor.
    box.reset(7, 14);
    CUICellItem suit(2, 3, 4);
    assert(list.PredictDrop(&suit, box.aim(1, 2), cells) == dpPlace);
    assert(cells == want.set(1, 2, 2, 4));

    // A footprint running off the right edge cannot be placed.
    assert(list.PredictDrop(&suit, box.aim(6, 2), cells) == dpAuto);

    // Vertical lists swap the footprint, the way PlaceItemAtPos does.
    list.vertical = true;
    box.reset(7, 14);
    assert(list.PredictDrop(&suit, box.aim(1, 2), cells) == dpPlace);
    assert(cells == want.set(1, 2, 3, 3));
    list.vertical = false;

    // Grouping wins over position: the item merges into the stack it matches, wherever
    // the cursor is, and the preview points at that stack.
    box.reset(7, 14);
    list.grouping = true;
    CUICellItem stack(1, 1, 1);
    box.put(&stack, 2, 3);
    assert(list.PredictDrop(&medkit, box.aim(6, 12), cells) == dpMerge);
    assert(cells == want.set(2, 3, 2, 3));

    // A different item still lands under the cursor.
    CUICellItem bolt(1, 1, 9);
    assert(list.PredictDrop(&bolt, box.aim(6, 12), cells) == dpPlace);
    assert(cells == want.set(6, 12, 6, 12));

    // Dragging the stack itself inside its own list must not match itself.
    assert(list.PredictDrop(&stack, box.aim(6, 12), cells, &stack) == dpPlace);
    assert(cells == want.set(6, 12, 6, 12));

    // Two identical stacks: the one that is not being dragged is the merge target.
    CUICellItem other(1, 1, 1);
    box.put(&other, 4, 4);
    assert(list.PredictDrop(&stack, box.aim(6, 12), cells, &stack) == dpMerge);
    assert(cells == want.set(4, 4, 4, 4));
    list.grouping = false;

    // ---- the highlight lands on the cell it claims --------------------------------
    // The preview is drawn from its own copy of the cell geometry; these checks pin it
    // to the pixels the grid pass itself uses for the same cell.
    {
        Scene s(7, 14, 41, 0);
        CUICellItem pill(1, 1, 5);
        s.run(&pill, 4, 6);

        assert(capture.batches() == 2);
        Fvector2 tp;
        s.box.GetTexUVLT(tp, 4, 6, 0);
        const std::vector<Point> grid_quad = quad_for(capture.of(0), tp);
        const std::vector<Point> preview = capture.of(1);
        assert(!grid_quad.empty());
        same_pixels(grid_quad, preview);
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
        Fvector2 tp;
        s.box.GetTexUVLT(tp, 4, 6, 0);
        same_pixels(quad_for(capture.of(0), tp), capture.of(1));
    }

    // Spaced list, like the belt: cell pitch is not the cell size.
    {
        Scene s(5, 1, 41, 24);
        CUICellItem pill(1, 1, 5);
        const Ivector2 cell = s.box.PickCell(s.box.aim(3, 0));
        s.run(&pill, 3, 0);

        assert(capture.batches() == 2);
        Fvector2 tp;
        s.box.GetTexUVLT(tp, u32(cell.x), u32(cell.y), 0);
        same_pixels(quad_for(capture.of(0), tp), capture.of(1));
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
            Fvector2 tp;
            s.box.GetTexUVLT(tp, u32(2 + i), 8, 0);
            same_pixels(quad_for(capture.of(0), tp), quad_for(preview, tp));
        }
    }

    // Blocked cells get the other tint.
    {
        Scene s(7, 14, 41, 0);
        CUICellItem pill(1, 1, 5);
        CUICellItem taken(1, 1, 7);
        s.box.put(&taken, 4, 6);
        s.run(&pill, 4, 6);

        assert(capture.batches() == 2);
        assert(capture.of(1)[0].color == kDropPreviewBlocked);
        assert(kDropPreviewBlocked != kDropPreviewFree);
        // Drawn after the item, so the tint is not hidden under the icon.
        assert(taken.drawn == 1);
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

print('PASS: production PredictDrop and the production Draw/DrawDropPreview pass; free cell, '
      'blocked cell, off-grid cursor, self-overlap while moving a 2x1, multi-cell footprint, '
      'vertical swap, grouping merge target and self-match; highlight pixels matched against the '
      'grid cell for plain, scrolled, spaced and 2x1 cases; no highlight for virtual cells, '
      'foreign list, fixed placement, single cell list; ASan/UBSan')
