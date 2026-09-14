"""Compile the production grab-offset quantisation and cell picking, then check
that the cell an item is dropped into is the cell under the cursor."""
from pathlib import Path
import os
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


cell_item = (root / 'src/xrGame/ui/UICellItem.cpp').read_text()
drag_drop = (root / 'src/xrGame/ui/UIDragDropListEx.cpp').read_text()

quantize = body(cell_item, 'Fvector2 quantize_grab_offset(')
pick_cell = body(drag_drop, 'Ivector2 CUICellContainer::PickCell(')
valid_cell = body(drag_drop, 'bool CUICellContainer::ValidCell(')
metrics = body(drag_drop, 'bool CUICellContainer::UpdateCellMetrics(')
cell_offset = body(drag_drop, 'Fvector2 CUICellContainer::CellOffsetUI(')

# kCellPixelBias and screen_cell_len live in the anonymous namespace at the top.
helper = drag_drop[drag_drop.index('constexpr float kCellPixelBias'):drag_drop.index('IC int screen_cell_len(')]
helper += 'IC int screen_cell_len(int ui_len, float scale)' + body(drag_drop, 'IC int screen_cell_len(')

code = r'''
#include <cassert>
#include <cmath>
#include <cstdio>

struct Fvector2
{
    float x, y;
    Fvector2& set(float a, float b) { x = a; y = b; return *this; }
    Fvector2& sub(const Fvector2& a, const Fvector2& b) { x = a.x - b.x; y = a.y - b.y; return *this; }
    Fvector2& sub(const Fvector2& o) { x -= o.x; y -= o.y; return *this; }
    Fvector2& add(const Fvector2& a, const Fvector2& b) { x = a.x + b.x; y = a.y + b.y; return *this; }
    Fvector2& mul(float s) { x *= s; y *= s; return *this; }
};

struct Ivector2
{
    int x, y;
    Ivector2& set(int a, int b) { x = a; y = b; return *this; }
    bool operator==(const Ivector2& o) const { return x == o.x && y == o.y; }
};

static int iFloor(float f) { return (int)std::floor(f); }
template <class T> static T _max(T a, T b) { return a > b ? a : b; }
template <class T> static void clamp(T& v, const T& lo, const T& hi) { if (v < lo) v = lo; else if (v > hi) v = hi; }
#define IC inline

// The device scale the list builds its effective cell size from.
static float g_scale_x = 1.0f, g_scale_y = 1.0f;
struct UiCore
{
    float ClientToScreenScaledX(float v) const { return v * g_scale_x; }
    float ClientToScreenScaledY(float v) const { return v * g_scale_y; }
};
static UiCore ui_core_instance;
static UiCore& UI() { return ui_core_instance; }

HELPER_BODY

Fvector2 quantize_grab_offset(const Fvector2& grab_offset, const Fvector2& cell_size, const Ivector2& grid_size)
QUANTIZE_BODY

// The container the engine picks the destination cell from. Every body below is the
// production one, so a change to the picking maths shows up here.
struct Grid
{
    Ivector2 m_cellsCapacity, m_cellSizeRaw, m_cellSpacingRaw, m_cellSizeScreen, m_cellSpacingScreen;
    Fvector2 m_cellSize, m_cellSpacing, m_metricsScale;
    Fvector2 origin;

    Grid()
    {
        m_cellsCapacity.set(0, 0);
        m_cellSizeRaw.set(0, 0);
        m_cellSpacingRaw.set(0, 0);
        m_cellSizeScreen.set(0, 0);
        m_cellSpacingScreen.set(0, 0);
        m_cellSize.set(0.0f, 0.0f);
        m_cellSpacing.set(0.0f, 0.0f);
        m_metricsScale.set(1.0f, 1.0f);
        origin.set(0.0f, 0.0f);
    }

    void GetAbsolutePos(Fvector2& p) { p = origin; }
    bool UpdateCellMetrics() METRICS_BODY
    Fvector2 CellOffsetUI(const Ivector2& cell_pos) const OFFSET_BODY
    bool ValidCell(const Ivector2& pos) const VALID_BODY
    Ivector2 PickCell(const Fvector2& abs_pos) PICK_BODY

    Fvector2 cell_lt(int cx, int cy) const
    {
        Ivector2 c;
        c.set(cx, cy);
        Fvector2 p;
        p.add(origin, CellOffsetUI(c));
        return p;
    }
};

// One full drag: Init() captures the grab offset, GetPosition() adds it back to the
// cursor and SetItem() picks the cell from that position. `snap` off reproduces the
// behaviour before the fix, where the raw grab offset was used.
static Ivector2 drop_cell(Grid& g, Fvector2 icon_lt, Ivector2 grid_size,
                          Fvector2 grab_cursor, Fvector2 drop_cursor, bool snap)
{
    Fvector2 grab_offset;
    grab_offset.sub(icon_lt, grab_cursor);
    const Fvector2 drop_offset = snap ? quantize_grab_offset(grab_offset, g.m_cellSize, grid_size) : grab_offset;
    Fvector2 pos;
    pos.add(drop_offset, drop_cursor);
    return g.PickCell(pos);
}

int main()
{
    // gamedata/configs/ui/actor_menu.xml, dragdrop_bag: 7x14 cells of 41x41, no spacing.
    Grid g;
    g.m_cellSizeRaw.set(41, 41);
    g.m_cellSpacingRaw.set(0, 0);
    g.m_cellsCapacity.set(7, 14);
    g.origin.set(702.0f, 119.0f);
    assert(g.UpdateCellMetrics());
    assert(g.m_cellSizeScreen.x == 41 && g.m_cellSizeScreen.y == 41);

    const float grabs[] = { 0.5f, 20.5f, 40.5f };   // corner, centre and far edge of the icon
    const float aims[] = { 0.5f, 20.5f, 40.5f };    // where inside the target cell the mouse is released
    Ivector2 one;
    one.set(1, 1);

    // 1x1 item: whatever part of the icon was grabbed, the cell under the cursor wins.
    int checked = 0;
    for (float gx : grabs)
        for (float gy : grabs)
        {
            const Fvector2 icon_lt = g.cell_lt(1, 2);
            Fvector2 grab_cursor;
            grab_cursor.set(icon_lt.x + gx, icon_lt.y + gy);

            for (int cx = 0; cx < g.m_cellsCapacity.x; ++cx)
                for (int cy = 0; cy < g.m_cellsCapacity.y; ++cy)
                    for (float ax : aims)
                        for (float ay : aims)
                        {
                            const Fvector2 cell = g.cell_lt(cx, cy);
                            Fvector2 drop_cursor;
                            drop_cursor.set(cell.x + ax, cell.y + ay);

                            Ivector2 want;
                            want.set(cx, cy);
                            assert(drop_cell(g, icon_lt, one, grab_cursor, drop_cursor, true) == want);
                            ++checked;
                        }
        }
    assert(checked == 3 * 3 * 7 * 14 * 3 * 3);

    // The bug this replaces: grabbed in the middle, released in the upper left part of
    // a cell, the item used to land one cell up and to the left.
    {
        const Fvector2 icon_lt = g.cell_lt(1, 2);
        Fvector2 grab_cursor;
        grab_cursor.set(icon_lt.x + 20.5f, icon_lt.y + 20.5f);
        const Fvector2 cell = g.cell_lt(4, 5);
        Fvector2 drop_cursor;
        drop_cursor.set(cell.x + 0.5f, cell.y + 0.5f);

        Ivector2 old_target;
        old_target.set(3, 4);
        Ivector2 new_target;
        new_target.set(4, 5);
        assert(drop_cell(g, icon_lt, one, grab_cursor, drop_cursor, false) == old_target);
        assert(drop_cell(g, icon_lt, one, grab_cursor, drop_cursor, true) == new_target);
    }

    // 2x1 weapon: the grabbed half stays under the cursor, so the item's left cell is
    // the cursor cell minus the grabbed sub-cell.
    {
        Ivector2 two_by_one;
        two_by_one.set(2, 1);
        const Fvector2 icon_lt = g.cell_lt(0, 0);

        for (int sub = 0; sub < 2; ++sub)
            for (int cx = 2; cx < g.m_cellsCapacity.x; ++cx)
                for (float ax : aims)
                {
                    Fvector2 grab_cursor;
                    grab_cursor.set(icon_lt.x + float(sub * 41) + 20.5f, icon_lt.y + 20.5f);
                    const Fvector2 cell = g.cell_lt(cx, 3);
                    Fvector2 drop_cursor;
                    drop_cursor.set(cell.x + ax, cell.y + 20.5f);

                    Ivector2 want;
                    want.set(cx - sub, 3);
                    assert(drop_cell(g, icon_lt, two_by_one, grab_cursor, drop_cursor, true) == want);
                }
    }

    // 1x2 outfit: the same on the vertical axis, and the clamp is per axis.
    {
        Ivector2 one_by_two;
        one_by_two.set(1, 2);
        const Fvector2 icon_lt = g.cell_lt(0, 0);
        Fvector2 grab_cursor;
        grab_cursor.set(icon_lt.x + 20.5f, icon_lt.y + 41.0f + 20.5f);
        const Fvector2 cell = g.cell_lt(3, 7);
        Fvector2 drop_cursor;
        drop_cursor.set(cell.x + 20.5f, cell.y + 20.5f);

        Ivector2 want;
        want.set(3, 6);
        assert(drop_cell(g, icon_lt, one_by_two, grab_cursor, drop_cursor, true) == want);
    }

    // Cursor a hair outside the icon between the mouse move and CreateDragItem: the
    // sub-cell must clamp to zero instead of shifting the item one cell right/down.
    {
        const Fvector2 icon_lt = g.cell_lt(1, 2);
        Fvector2 grab_cursor;
        grab_cursor.set(icon_lt.x - 0.001f, icon_lt.y - 0.001f);
        const Fvector2 cell = g.cell_lt(5, 9);
        Fvector2 drop_cursor;
        drop_cursor.set(cell.x + 20.5f, cell.y + 20.5f);

        Ivector2 want;
        want.set(5, 9);
        assert(drop_cell(g, icon_lt, one, grab_cursor, drop_cursor, true) == want);
    }

    // A grab offset larger than the item never reaches past its last cell.
    {
        Ivector2 two_by_one;
        two_by_one.set(2, 1);
        Fvector2 huge;
        huge.set(-1000.0f, -1000.0f);
        Fvector2 cell41;
        cell41.set(41.0f, 41.0f);
        const Fvector2 res = quantize_grab_offset(huge, cell41, two_by_one);
        assert(res.x == -41.0f && res.y == 0.0f);
    }

    // Degenerate cell size leaves the offset untouched instead of dividing by zero.
    {
        Fvector2 raw;
        raw.set(-13.0f, -7.0f);
        Fvector2 none;
        none.set(0.0f, 0.0f);
        const Fvector2 res = quantize_grab_offset(raw, none, one);
        assert(res.x == raw.x && res.y == raw.y);
    }

    std::printf("ok\n");
    return 0;
}
'''

code = (code
        .replace('HELPER_BODY', helper)
        .replace('QUANTIZE_BODY', quantize)
        .replace('METRICS_BODY', metrics)
        .replace('OFFSET_BODY', cell_offset)
        .replace('VALID_BODY', valid_cell)
        .replace('PICK_BODY', pick_cell))

with tempfile.TemporaryDirectory(prefix='ixray-drop-cell-') as directory:
    source = Path(directory) / 'test.cpp'
    source.write_text(code)
    binary = source.with_suffix('')
    subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-g', str(source), '-o', str(binary)], check=True)
    # LeakSanitizer cannot run inside the desktop sandbox; retain address/UB checks.
    subprocess.run([str(binary)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS=os.environ.get('ASAN_OPTIONS', 'detect_leaks=0')))

print('PASS: production quantize_grab_offset + PickCell; 7938 cursor positions for a 1x1 item '
      'over three grab points, separate 2x1 and 1x2 grabs, pre-fix offset reproduced, clamp on '
      'drifted and oversized grabs, zero cell size; ASan/UBSan')
