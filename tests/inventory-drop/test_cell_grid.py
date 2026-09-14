"""Compile the production cell metrics, item placement and grid drawing, then check that
one drag&drop list steps by one constant whole number of screen pixels: that the grid
background, the item rectangles and PickCell all agree on it, and that neighbouring item
icons share an edge exactly instead of gaping or overlapping by a pixel.

The icon rasterization itself (CUIStaticItem::RenderInternal) is modelled here, not
compiled - compiling it drags in sPoly2D, ClipPoly and the renderer. So this checks the
model of the rasterizer; only a run of the game checks the rasterizer."""
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

constants = '\n'.join(re.findall(r'^constexpr (?:float|u32) k\w+\s*=.*?;$', drag_drop, re.M))
assert 'kCellPixelBias' in constants and 'kInventoryCellUSpanGridDisabled' in constants, constants

bodies = {
    'SCREEN_LEN_BODY': body(drag_drop, 'IC int screen_cell_len('),
    'METRICS_BODY': body(drag_drop, 'bool CUICellContainer::UpdateCellMetrics('),
    'OFFSET_BODY': body(drag_drop, 'Fvector2 CUICellContainer::CellOffsetUI('),
    'GEOMETRY_BODY': body(drag_drop, 'void CUICellContainer::SetItemGeometry('),
    'REFRESH_BODY': body(drag_drop, 'void CUICellContainer::RefreshItemsPos('),
    'REINIT_BODY': body(drag_drop, 'void CUICellContainer::ReinitSize('),
    'VALID_BODY': body(drag_drop, 'bool CUICellContainer::ValidCell('),
    'CELL_AT_BODY': body(drag_drop, 'CUICell& CUICellContainer::GetCellAt('),
    'PICK_BODY': body(drag_drop, 'Ivector2 CUICellContainer::PickCell('),
    'TOP_CELL_BODY': body(drag_drop, 'Ivector2 CUICellContainer::TopVisibleCell('),
    'TEX_UV_BODY': body(drag_drop, 'void CUICellContainer::GetTexUVLT('),
    'IN_RANGE_BODY': body(drag_drop, 'u32 CUICellContainer::GetCellsInRange('),
    'DRAW_BODY': body(drag_drop, 'void CUICellContainer::Draw('),
    'PREVIEW_BODY': body(drag_drop, 'void CUICellContainer::DrawDropPreview('),
}

code = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include <utility>
#include <algorithm>

#define R_ASSERT(x) assert(x)
#define IC inline
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
    Fvector2& mul(const Fvector2& o) { x *= o.x; y *= o.y; return *this; }
    Fvector2& div(float s) { x /= s; y /= s; return *this; }
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
};
typedef Rect<int> Irect;
typedef Rect<float> Frect;

static int iFloor(float f) { return (int)std::floor(f); }
template <class T> static T _min(T a, T b) { return a < b ? a : b; }
template <class T> static T _max(T a, T b) { return a > b ? a : b; }
template <class T> static void clamp(T& v, const T& lo, const T& hi) { if (v < lo) v = lo; else if (v > hi) v = hi; }
struct xrCriticalSectionGuard { explicit xrCriticalSectionGuard(int&) {} };

enum EDropPreview { dpMerge, dpPlace, dpAuto };

// --- render capture -------------------------------------------------------------
struct Point { int batch; float x, y, z; u32 color; float u, v; };
struct IUIRender { enum ePrimitiveType { ptTriList }; enum ePointType { pttLIT }; };

struct RenderCapture
{
    std::vector<Point> points;
    int batch = -1;
    void StartPrimitive(u32, int, int) { ++batch; }
    void PushPoint(float x, float y, float z, u32 c, float u, float v) { points.push_back(Point{batch, x, y, z, c, u, v}); }
    void SetShader(int) {}
    void FlushPrimitive() {}
    void reset() { points.clear(); batch = -1; }
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

// The engine scale is a plain per-axis multiply, set per test case.
static float g_scale_x = 1.0f, g_scale_y = 1.0f;
struct UiCore
{
    int m_currentPointType = 0;
    Frect scissor;
    float ClientToScreenScaledX(float v) const { return v * g_scale_x; }
    float ClientToScreenScaledY(float v) const { return v * g_scale_y; }
    void ClientToScreenScaled(Fvector2& dest, float left, float top) const { dest.set(left * g_scale_x, top * g_scale_y); }
    void PushScissor(const Frect& r) { scissor = r; }
    void PopScissor() {}
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
    Fvector2 wnd_size{0.0f, 0.0f};
    int reinit_scrolls = 0;

    bool IsGrouping() { return grouping; }
    bool GetVerticalPlacement() { return vertical; }
    bool GetVirtualCells() { return virtual_cells; }
    bool GetCustomPlacement() { return custom_placement; }
    int ScrollPos() { return scroll_pos; }
    void GetClientArea(Frect& r) { r = client_area; }
    const Fvector2& GetWndSize() { return wnd_size; }
    void ReinitScroll() { ++reinit_scrolls; }

    const Ivector2& CellsCapacity();
    EDropPreview PredictDrop(CUICellItem*, const Fvector2&, Irect& out_cells, CUICellItem* = nullptr)
    {
        out_cells.set(0, 0, -1, -1);
        return dpAuto;
    }
};

struct CUIWindow { virtual ~CUIWindow() {} };

struct CUICellItem : CUIWindow
{
    Ivector2 grid;
    Fvector2 wnd_pos{0.0f, 0.0f}, wnd_size{0.0f, 0.0f};
    bool m_cur_mark = false, m_selected = false, m_select_armament = false, m_select_equipped = false;
    u32 m_drawn_frame = 0;
    CUICellItem(int w, int h) { grid.set(w, h); }
    const Ivector2& GetGridSize() { return grid; }
    CUIDragDropListEx* OwnerList() { return nullptr; }
    void SetWndPos(const Fvector2& p) { wnd_pos = p; }
    void SetWndSize(const Fvector2& s) { wnd_size = s; }
    void Draw() {}
};

struct CUIDragItem
{
    CUIDragDropListEx* BackList() { return nullptr; }
    CUICellItem* ParentItem() { return nullptr; }
    Fvector2 GetPosition() { Fvector2 p; p.set(0.0f, 0.0f); return p; }
};
CUIDragItem* CUIDragDropListEx::m_drag_item = nullptr;

struct CUICell
{
    CUICellItem* m_item;
    bool m_bMainItem;
    CUICell() : m_item(nullptr), m_bMainItem(false) {}
    bool Empty() { return m_item == nullptr; }
    bool MainItem() { return m_bMainItem; }
    bool operator==(const CUICell& o) const { return m_item == o.m_item; }
};
typedef std::vector<CUICell> UI_CELLS_VEC;
typedef UI_CELLS_VEC::iterator UI_CELLS_VEC_IT;

struct CUICellContainer
{
    typedef CUICellContainer inherited;   // Update() is not exercised here

    CUIDragDropListEx* m_pParentDragDropList = nullptr;
    Ivector2 m_cellsCapacity{0, 0};
    Ivector2 m_cellSizeRaw{0, 0}, m_cellSpacingRaw{0, 0};
    Ivector2 m_cellSizeScreen{0, 0}, m_cellSpacingScreen{0, 0};
    Fvector2 m_cellSize{0.0f, 0.0f}, m_cellSpacing{0.0f, 0.0f}, m_metricsScale{1.0f, 1.0f};
    Fvector2 origin{0.0f, 0.0f}, wnd_size{0.0f, 0.0f};
    bool m_isInventoryGridDisabled = true;
    ui_shader hShader;
    UI_CELLS_VEC m_cells, m_cells_to_draw;
    int csUi = 0;

    void GetAbsolutePos(Fvector2& p) { p = origin; }
    void SetWndSize(const Fvector2& s) { wnd_size = s; }
    const Fvector2& CellSize() { return m_cellSize; }
    const Fvector2& CellsSpacing() { return m_cellSpacing; }
    const Ivector2& CellsCapacity() { return m_cellsCapacity; }

    bool UpdateCellMetrics() METRICS_BODY
    Fvector2 CellOffsetUI(const Ivector2& cell_pos) const OFFSET_BODY
    void SetItemGeometry(CUICellItem* itm, const Ivector2& cell_pos) GEOMETRY_BODY
    void RefreshItemsPos() REFRESH_BODY
    void ReinitSize() REINIT_BODY
    bool ValidCell(const Ivector2& pos) const VALID_BODY
    CUICell& GetCellAt(const Ivector2& pos) CELL_AT_BODY
    Ivector2 PickCell(const Fvector2& abs_pos) PICK_BODY
    Ivector2 TopVisibleCell() TOP_CELL_BODY
    void GetTexUVLT(Fvector2& uv, u32 col, u32 row, u8 select_mode) TEX_UV_BODY
    u32 GetCellsInRange(const Irect& rect, UI_CELLS_VEC& res) IN_RANGE_BODY
    void Draw() DRAW_BODY
    void DrawDropPreview(const Irect& tgt_cells, const Fvector2& draw_lt, const Fvector2& f_len, const Fvector2& sp_len) PREVIEW_BODY

    // Test scaffolding, not production code.
    void reset(int cols, int rows)
    {
        m_cellsCapacity.set(cols, rows);
        m_cells.assign(size_t(cols * rows), CUICell());
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
                GetCellAt(p).m_bMainItem = (x == 0 && y == 0);
            }
        Ivector2 at;
        at.set(cx, cy);
        SetItemGeometry(itm, at);
    }
};

const Ivector2& CUIDragDropListEx::CellsCapacity() { return m_container->m_cellsCapacity; }

// --- the rasterizer, modelled -----------------------------------------------------
// CUIStaticItem::RenderInternal floors the scaled top left corner and then adds the
// scaled size to it without flooring. That asymmetry is the whole reason a fractional
// cell size makes neighbouring icons gape and overlap.
struct Rasterized { float x1, y1, x2, y2; };
static Rasterized rasterize(const Fvector2& abs_pos, const Fvector2& size)
{
    Rasterized r;
    r.x1 = float(iFloor(abs_pos.x * g_scale_x));
    r.y1 = float(iFloor(abs_pos.y * g_scale_y));
    r.x2 = r.x1 + size.x * g_scale_x;
    r.y2 = r.y1 + size.y * g_scale_y;
    return r;
}

struct Scene
{
    CUIDragDropListEx list;
    CUICellContainer box;

    Scene(int cols, int rows, int cell_w, int cell_h, int sp_x, int sp_y)
    {
        list.m_container = &box;
        box.m_pParentDragDropList = &list;
        box.m_cellSizeRaw.set(cell_w, cell_h);
        box.m_cellSpacingRaw.set(sp_x, sp_y);
        box.UpdateCellMetrics();
        box.origin.set(702.0f, 119.0f);
        box.reset(cols, rows);
        box.ReinitSize();
        list.wnd_size = box.wnd_size;
        list.client_area.set(box.origin.x, box.origin.y,
                             box.origin.x + box.wnd_size.x, box.origin.y + box.wnd_size.y);
    }

    // Absolute UI position of an item sitting in a cell, the way the engine builds it.
    Fvector2 item_abs(const CUICellItem& itm) const
    {
        Fvector2 p;
        p.add(box.origin, itm.wnd_pos);
        return p;
    }
};

// The grid pass pushes six vertices per cell, all at iFloor(...) - 0.5f.
static Rasterized quad_bounds(const std::vector<Point>& pts, size_t at)
{
    Rasterized r{pts[at].x, pts[at].y, pts[at].x, pts[at].y};
    for (size_t k = at; k < at + 6; ++k)
    {
        r.x1 = _min(r.x1, pts[k].x);
        r.y1 = _min(r.y1, pts[k].y);
        r.x2 = _max(r.x2, pts[k].x);
        r.y2 = _max(r.y2, pts[k].y);
    }
    return r;
}

static Rasterized grid_quad(const std::vector<Point>& pts, const Fvector2& tp)
{
    for (size_t i = 0; i + 6 <= pts.size(); i += 6)
        if (std::fabs(pts[i].u - tp.x) < 1e-6f && std::fabs(pts[i].v - tp.y) < 1e-6f)
            return quad_bounds(pts, i);
    assert(false && "cell was not drawn");
    return Rasterized{0.0f, 0.0f, 0.0f, 0.0f};
}

struct Mode { float sx, sy; const char* name; };

int main()
{
    // ---- the effective cell size is a whole number of screen pixels ---------------
    {
        // The reported case: a 48 px cell at a scale of 1.7 is 81.6 px on screen.
        g_scale_x = g_scale_y = 1.7f;
        Scene s(7, 14, 48, 48, 0, 0);
        assert(s.box.m_cellSizeScreen.x == 82 && s.box.m_cellSizeScreen.y == 82);

        // and the grid steps by exactly that, on both axes.
        CUICellItem a(1, 1), b(1, 1), c(1, 1);
        s.box.put(&a, 0, 0);
        s.box.put(&b, 1, 0);
        s.box.put(&c, 0, 1);
        const Rasterized ra = rasterize(s.item_abs(a), a.wnd_size);
        const Rasterized rb = rasterize(s.item_abs(b), b.wnd_size);
        const Rasterized rc = rasterize(s.item_abs(c), c.wnd_size);
        assert(rb.x1 - ra.x1 == 82.0f);
        assert(rc.y1 - ra.y1 == 82.0f);
        assert(ra.x2 - ra.x1 == 82.0f && ra.y2 - ra.y1 == 82.0f);
    }

    // Rounding is to nearest, per axis, and a cell never collapses to nothing.
    {
        g_scale_x = 1.875f;   // 1920 / 1024
        g_scale_y = 1.40625f; // 1080 / 768
        Scene s(7, 14, 33, 41, 0, 0);
        assert(s.box.m_cellSizeScreen.x == iFloor(33.0f * 1.875f + 0.5f));      // 61.875 -> 62
        assert(s.box.m_cellSizeScreen.y == iFloor(41.0f * 1.40625f + 0.5f));    // 57.656 -> 58
        assert(s.box.m_cellSpacingScreen.x == 0 && s.box.m_cellSpacingScreen.y == 0);
    }
    {
        g_scale_x = g_scale_y = 0.01f;
        Scene s(2, 2, 1, 1, 0, 0);
        assert(s.box.m_cellSizeScreen.x == 1 && s.box.m_cellSizeScreen.y == 1);
    }

    // A device that has not reported a size yet must not leave a zero divisor behind:
    // CellOffsetUI divides by the scale, and an infinity there poisons every position.
    {
        g_scale_x = 1.875f;
        g_scale_y = 1.40625f;
        Scene s(7, 14, 33, 41, 0, 0);
        const Ivector2 built = s.box.m_cellSizeScreen;
        const Fvector2 scale = s.box.m_metricsScale;

        g_scale_x = 0.0f;
        g_scale_y = 0.0f;
        assert(!s.box.UpdateCellMetrics());
        assert(s.box.m_cellSizeScreen == built);
        assert(s.box.m_metricsScale.x == scale.x && s.box.m_metricsScale.y == scale.y);

        Ivector2 at;
        at.set(3, 4);
        const Fvector2 off = s.box.CellOffsetUI(at);
        assert(std::isfinite(off.x) && std::isfinite(off.y));
    }

    // ---- one constant step, whatever the mode --------------------------------------
    const Mode modes[] = {
        {1.0f, 1.0f, "1024x768"},
        {1.333984375f, 1.0f, "1366x768"},
        {1.875f, 1.40625f, "1920x1080"},
        {2.5f, 1.875f, "2560x1440"},
        {3.359375f, 1.875f, "3440x1440"},
        {1.7f, 1.7f, "the reported 81.6 px cell"},
    };

    // Every shipped list shape: bag, the 4:3 bag, the belt and the quick slots of both
    // aspect ratios, and the one-cell trash panel.
    struct Shape { int cols, rows, cw, ch, spx, spy; };
    const Shape shapes[] = {
        {7, 14, 33, 41, 0, 0},
        {7, 14, 41, 41, 0, 0},
        {5, 1, 33, 41, 19, 0},
        {5, 1, 41, 41, 24, 0},
        {4, 1, 33, 41, 32, 0},
        {4, 1, 41, 41, 40, 0},
        {1, 1, 375, 768, 0, 0},
        {5, 1, 32, 32, 0, 33},   // gamedata_cs vertical belt, spacing on y
    };

    for (const Mode& m : modes)
        for (const Shape& sh : shapes)
        {
            g_scale_x = m.sx;
            g_scale_y = m.sy;
            Scene s(sh.cols, sh.rows, sh.cw, sh.ch, sh.spx, sh.spy);

            const int step_x = s.box.m_cellSizeScreen.x + s.box.m_cellSpacingScreen.x;
            const int step_y = s.box.m_cellSizeScreen.y + s.box.m_cellSpacingScreen.y;

            std::vector<CUICellItem> items;
            items.reserve(size_t(sh.cols * sh.rows));
            for (int i = 0; i < sh.cols * sh.rows; ++i)
                items.push_back(CUICellItem(1, 1));
            for (int cy = 0; cy < sh.rows; ++cy)
                for (int cx = 0; cx < sh.cols; ++cx)
                    s.box.put(&items[size_t(cy * sh.cols + cx)], cx, cy);

            // 1. The step between neighbouring items is constant and whole.
            for (int cy = 0; cy < sh.rows; ++cy)
                for (int cx = 0; cx < sh.cols; ++cx)
                {
                    const Rasterized r = rasterize(s.item_abs(items[size_t(cy * sh.cols + cx)]),
                                                   items[size_t(cy * sh.cols + cx)].wnd_size);
                    if (cx > 0)
                    {
                        const Rasterized l = rasterize(s.item_abs(items[size_t(cy * sh.cols + cx - 1)]),
                                                       items[size_t(cy * sh.cols + cx - 1)].wnd_size);
                        assert(r.x1 - l.x1 == float(step_x));
                    }
                    if (cy > 0)
                    {
                        const Rasterized u = rasterize(s.item_abs(items[size_t((cy - 1) * sh.cols + cx)]),
                                                       items[size_t((cy - 1) * sh.cols + cx)].wnd_size);
                        assert(r.y1 - u.y1 == float(step_y));
                    }

                    // 2. And the item covers exactly one cell, so with no spacing its
                    // right edge is the left edge of its neighbour - the reported bug.
                    assert(std::fabs((r.x2 - r.x1) - float(s.box.m_cellSizeScreen.x)) < 0.05f);
                    assert(std::fabs((r.y2 - r.y1) - float(s.box.m_cellSizeScreen.y)) < 0.05f);
                }

            // 3. The grid background lands on the very same pixels as the items.
            capture.reset();
            s.box.Draw();
            const std::vector<Point> grid = capture.of(0);
            for (int cy = 0; cy < sh.rows; ++cy)
                for (int cx = 0; cx < sh.cols; ++cx)
                {
                    Fvector2 tp;
                    s.box.GetTexUVLT(tp, u32(cx), u32(cy), 0);
                    const Rasterized q = grid_quad(grid, tp);
                    const Rasterized r = rasterize(s.item_abs(items[size_t(cy * sh.cols + cx)]),
                                                   items[size_t(cy * sh.cols + cx)].wnd_size);
                    // The grid pushes its vertices at iFloor(...) - 0.5f.
                    assert(q.x1 + 0.5f == r.x1);
                    assert(q.y1 + 0.5f == r.y1);
                    assert(q.x2 - q.x1 == float(s.box.m_cellSizeScreen.x));
                    assert(q.y2 - q.y1 == float(s.box.m_cellSizeScreen.y));
                }

            // 4. PickCell resolves every drawn cell to itself, sampled across it.
            for (int cy = 0; cy < sh.rows; ++cy)
                for (int cx = 0; cx < sh.cols; ++cx)
                {
                    Ivector2 at;
                    at.set(cx, cy);
                    Fvector2 lt;
                    lt.add(s.box.origin, s.box.CellOffsetUI(at));
                    for (int px = 0; px < 8; ++px)
                        for (int py = 0; py < 8; ++py)
                        {
                            Fvector2 p;
                            p.set(lt.x + s.box.m_cellSize.x * (float(px) + 0.5f) / 8.0f,
                                  lt.y + s.box.m_cellSize.y * (float(py) + 0.5f) / 8.0f);
                            Ivector2 want;
                            want.set(cx, cy);
                            assert(s.box.PickCell(p) == want);
                        }
                }
        }

    // ---- multi-cell items are exact multiples ---------------------------------------
    {
        g_scale_x = g_scale_y = 1.7f;
        Scene s(7, 14, 48, 48, 0, 0);
        const int cell = s.box.m_cellSizeScreen.x;
        assert(cell == 82);

        CUICellItem gun(2, 1), suit(5, 2);
        s.box.put(&gun, 0, 0);
        s.box.put(&suit, 0, 4);

        const Rasterized rg = rasterize(s.item_abs(gun), gun.wnd_size);
        assert(std::fabs((rg.x2 - rg.x1) - 164.0f) < 0.05f);
        assert(std::fabs((rg.y2 - rg.y1) - 82.0f) < 0.05f);

        const Rasterized rs = rasterize(s.item_abs(suit), suit.wnd_size);
        assert(std::fabs((rs.x2 - rs.x1) - 410.0f) < 0.05f);
        assert(std::fabs((rs.y2 - rs.y1) - 164.0f) < 0.05f);
    }

    // ---- a scrolled list keeps the same step ----------------------------------------
    {
        g_scale_x = 1.875f;
        g_scale_y = 1.40625f;
        Scene s(7, 14, 33, 41, 0, 0);
        s.list.client_area.set(s.box.origin.x, s.box.origin.y,
                               s.box.origin.x + s.box.wnd_size.x,
                               s.box.origin.y + s.box.m_cellSize.y * 6.0f);
        s.list.scroll_pos = iFloor(3.0f * s.box.m_cellSize.y) + 1;
        s.box.origin.y -= float(s.list.scroll_pos);     // OnScrollV moves the container
        assert(s.box.TopVisibleCell().y == 3);

        std::vector<CUICellItem> items;
        items.reserve(14);
        for (int i = 0; i < 14; ++i)
            items.push_back(CUICellItem(1, 1));
        for (int cy = 0; cy < 14; ++cy)
            s.box.put(&items[size_t(cy)], 2, cy);

        capture.reset();
        s.box.Draw();
        const std::vector<Point> grid = capture.of(0);
        for (int cy = 3; cy < 9; ++cy)
        {
            Fvector2 tp;
            s.box.GetTexUVLT(tp, 2, u32(cy), 0);
            const Rasterized q = grid_quad(grid, tp);
            const Rasterized r = rasterize(s.item_abs(items[size_t(cy)]), items[size_t(cy)].wnd_size);
            assert(q.x1 + 0.5f == r.x1);
            assert(q.y1 + 0.5f == r.y1);
        }
    }

    // ---- a resolution change rebuilds the metrics, the extent and the items ---------
    {
        g_scale_x = 1.875f;
        g_scale_y = 1.40625f;
        Scene s(7, 14, 33, 41, 0, 0);

        CUICellItem pill(1, 1);
        s.box.put(&pill, 4, 6);
        const Fvector2 before_pos = pill.wnd_pos;
        const Fvector2 before_extent = s.box.wnd_size;
        const int before_cell = s.box.m_cellSizeScreen.x;

        assert(!s.box.UpdateCellMetrics());     // same scale, nothing to do

        g_scale_x = 2.5f;
        g_scale_y = 1.875f;
        assert(s.box.UpdateCellMetrics());
        s.box.ReinitSize();
        s.box.RefreshItemsPos();

        assert(s.box.m_cellSizeScreen.x == iFloor(33.0f * 2.5f + 0.5f));    // 82.5 -> 83
        assert(s.box.m_cellSizeScreen.x != before_cell);
        assert(before_pos.x != pill.wnd_pos.x || before_pos.y != pill.wnd_pos.y);
        assert(before_extent.x != s.box.wnd_size.x || before_extent.y != s.box.wnd_size.y);

        // The container extent is still exactly the grid it draws.
        assert(std::fabs(s.box.wnd_size.x * g_scale_x - float(7 * s.box.m_cellSizeScreen.x)) < 0.05f);
        assert(std::fabs(s.box.wnd_size.y * g_scale_y - float(14 * s.box.m_cellSizeScreen.y)) < 0.05f);

        // And the item is back on the grid.
        const Rasterized r = rasterize(s.item_abs(pill), pill.wnd_size);
        assert(std::fabs((r.x2 - r.x1) - float(s.box.m_cellSizeScreen.x)) < 0.05f);
    }

    // ---- vertical placement keeps its square-cell shape -----------------------------
    // PlaceItemAtPos uses the cell height on both axes there ("quads cells"), so the
    // horizontal extent is deliberately not pixel snapped. Pin the behaviour so the
    // limitation stays visible.
    {
        g_scale_x = 1.875f;
        g_scale_y = 1.40625f;
        Scene s(1, 5, 33, 41, 0, 0);
        s.list.vertical = true;
        CUICellItem pill(1, 1);
        s.box.put(&pill, 0, 2);
        assert(pill.wnd_size.x == s.box.m_cellSize.y);
        assert(pill.wnd_size.y == s.box.m_cellSize.y);
    }

    // ---- the behaviour this replaces --------------------------------------------------
    // Before the fix an item sat at (cellFromXml + spacing) * k UI units and was
    // cellFromXml wide, so the scaled step was fractional. Reproduce it and show that the
    // steps between neighbouring icons were not all the same and that edges did not meet -
    // otherwise the checks above would pass on a grid that never had the problem.
    {
        g_scale_x = 1.875f;
        g_scale_y = 1.40625f;
        Scene s(7, 14, 33, 41, 0, 0);

        bool step_varies = false, edge_missed = false;
        float prev_x1 = 0.0f, prev_x2 = 0.0f, first_step = 0.0f;
        for (int cx = 0; cx < 7; ++cx)
        {
            Fvector2 pos, size;
            pos.set(s.box.origin.x + float((s.box.m_cellSpacingRaw.x + s.box.m_cellSizeRaw.x) * cx),
                    s.box.origin.y);
            size.set(float(s.box.m_cellSizeRaw.x), float(s.box.m_cellSizeRaw.y));
            const Rasterized r = rasterize(pos, size);
            if (cx == 1)
                first_step = r.x1 - prev_x1;
            if (cx > 1 && r.x1 - prev_x1 != first_step)
                step_varies = true;
            if (cx > 0 && std::fabs(r.x1 - prev_x2) > 0.01f)
                edge_missed = true;
            prev_x1 = r.x1;
            prev_x2 = r.x2;
        }
        assert(step_varies);
        assert(edge_missed);

        // The same row through the production geometry has neither problem.
        std::vector<CUICellItem> items;
        items.reserve(7);
        for (int i = 0; i < 7; ++i)
            items.push_back(CUICellItem(1, 1));
        for (int cx = 0; cx < 7; ++cx)
            s.box.put(&items[size_t(cx)], cx, 0);
        for (int cx = 1; cx < 7; ++cx)
        {
            const Rasterized l = rasterize(s.item_abs(items[size_t(cx - 1)]), items[size_t(cx - 1)].wnd_size);
            const Rasterized r = rasterize(s.item_abs(items[size_t(cx)]), items[size_t(cx)].wnd_size);
            assert(r.x1 - l.x1 == float(s.box.m_cellSizeScreen.x));
            assert(std::fabs(l.x2 - r.x1) < 0.01f);
        }
    }

    // ---- the belt's last pixel now resolves ------------------------------------------
    // The averaged divisor used to truncate to whole UI units, so the very last pixel of
    // the belt pointed at a cell that does not exist. With the effective size it does not.
    {
        g_scale_x = g_scale_y = 1.0f;
        Scene s(5, 1, 41, 41, 24, 0);
        Fvector2 last;
        last.set(s.box.origin.x + s.box.wnd_size.x - 0.5f, s.box.origin.y + 0.5f);
        Ivector2 want;
        want.set(4, 0);
        assert(s.box.PickCell(last) == want);
    }

    std::printf("ok\n");
    return 0;
}
'''

code = code.replace('CONSTANTS', constants)
for token, text in bodies.items():
    code = code.replace(token, text)

with tempfile.TemporaryDirectory(prefix='ixray-cell-grid-') as directory:
    source = Path(directory) / 'test.cpp'
    source.write_text(code)
    binary = source.with_suffix('')
    subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-g', str(source), '-o', str(binary)], check=True)
    # LeakSanitizer cannot run inside the desktop sandbox; retain address/UB checks.
    subprocess.run([str(binary)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS=os.environ.get('ASAN_OPTIONS', 'detect_leaks=0')))

print('PASS: production UpdateCellMetrics, CellOffsetUI, SetItemGeometry, RefreshItemsPos, '
      'ReinitSize, PickCell, TopVisibleCell and Draw; whole pixel cell size including the '
      'reported 81.6 case, one constant step for eight list shapes over six scales, item edges '
      'meeting exactly, grid background on the same pixels, PickCell over 64 points per cell, '
      '2x1 and 5x2 extents, a scrolled list, a scale change, a refused zero scale, vertical '
      'placement, the pre-fix '
      'varying step reproduced, the belt last pixel; ASan/UBSan')
