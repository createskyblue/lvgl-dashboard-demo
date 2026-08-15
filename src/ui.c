#include "ui.h"
#include <math.h>
#include <stdlib.h>

#ifndef LVGL_SIM_AXIS_OVERLAY
#define LVGL_SIM_AXIS_OVERLAY 0  /* 0: side column, 1: overlay on full chart */
#endif

#define HOR_RES 320
#define VER_RES 240

#define COL_BG       lv_color_hex(0xF5F7FA)
#define COL_CARD     lv_color_hex(0xFFFFFF)
#define COL_TEXT     lv_color_hex(0x1F2937)
#define COL_MUTED    lv_color_hex(0x6B7280)
#define COL_FAINT    lv_color_hex(0x9CA3AF)
#define COL_GREEN    lv_color_hex(0x10B981)
#define COL_GREEN_BG lv_color_hex(0xD1FAE5)
#define COL_GREEN_TX lv_color_hex(0x065F46)
#define COL_AMBER    lv_color_hex(0xF59E0B)
#define COL_AMBER_BG lv_color_hex(0xFEF3C7)
#define COL_AMBER_TX lv_color_hex(0x92400E)
#define COL_RED      lv_color_hex(0xEF4444)
#define COL_RED_BG   lv_color_hex(0xFEE2E2)
#define COL_RED_TX   lv_color_hex(0x991B1B)
#define COL_ICON_BG  lv_color_hex(0xF3F4F6)
#define COL_CHART    lv_color_hex(0x0EA5E9)
#define COL_GRID     lv_color_hex(0xD6DCE3)

#define POINTS 40
static int32_t co2_data[POINTS];

static void gen_data(void)
{
    unsigned s = 20260814u;
    for(int i = 0; i < POINTS; i++) {
        double t = (double)i / (POINTS - 1) * 24.0;
        double night = 380.0 * exp(-pow((t - 4.2) / 2.8, 2));
        double day   = 140.0 * exp(-pow((t - 15.5) / 4.0, 2));
        s = s * 1664525u + 1013904223u;
        double noise = (double)((s >> 8) % 41) - 20.0;
        double v = 520.0 + night + day + noise;
        if(v < 400) v = 400;
        if(v > 1200) v = 1200;
        co2_data[i] = (int32_t)(v + 0.5);
    }
    co2_data[POINTS - 1] = 642; /* current reading */
}

static void set_bg(lv_obj_t *o, lv_color_t color)
{
    lv_obj_set_style_bg_color(o, color, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
}

static lv_obj_t *mk_label(lv_obj_t *parent, const char *txt, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

static lv_obj_t *mk_pill(lv_obj_t *parent)
{
    lv_obj_t *pill = lv_obj_create(parent);
    lv_obj_remove_style_all(pill);
    set_bg(pill, COL_GREEN_BG);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(pill, 999, 0);
    lv_obj_set_style_pad_hor(pill, 8, 0);
    lv_obj_set_style_pad_ver(pill, 3, 0);
    lv_obj_set_flex_flow(pill, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(pill, 5, 0);
    return pill;
}

static lv_obj_t *mk_pill_dot(lv_obj_t *pill)
{
    lv_obj_t *dot = lv_obj_create(pill);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 6, 6);
    set_bg(dot, COL_GREEN);
    lv_obj_set_style_radius(dot, 3, 0);
    return dot;
}

static void set_pill(lv_obj_t *pill, lv_obj_t *dot, lv_obj_t *lbl,
                     const char *txt, lv_color_t bg, lv_color_t dotc, lv_color_t txc)
{
    set_bg(pill, bg);
    set_bg(dot, dotc);
    lv_label_set_text(lbl, txt);
    lv_obj_set_style_text_color(lbl, txc, 0);
}


/* ============ gradient-filled area chart (thorvg vector) ============ */

typedef struct {
    int32_t *data;       /* owned copy of the trend values */
    uint32_t n;
    lv_color_t color;
    lv_color_t bg;       /* card background used to erase above the curve */
    uint8_t grid;        /* number of horizontal gridlines */
    lv_color_t grid_color;
} area_ctx_t;

static void area_free_cb(lv_event_t *e)
{
    area_ctx_t *ctx = (area_ctx_t *)lv_event_get_user_data(e);
    if(ctx) {
        lv_free(ctx->data);
        lv_free(ctx);
    }
}

/* Catmull-Rom -> cubic Bezier control points for the segment [i, i+1] */
static void catmull_ctrl(const int32_t *xs, const int32_t *ys, int n, int i,
                         lv_fpoint_t *c1, lv_fpoint_t *c2)
{
    int i0 = i > 0 ? i - 1 : i;
    int i3 = i + 2 < n ? i + 2 : i + 1;
    c1->x = xs[i] + (float)(xs[i + 1] - xs[i0]) / 6.0f;
    c1->y = ys[i] + (float)(ys[i + 1] - ys[i0]) / 6.0f;
    c2->x = xs[i + 1] - (float)(xs[i3] - xs[i]) / 6.0f;
    c2->y = ys[i + 1] - (float)(ys[i3] - ys[i]) / 6.0f;
}

/* Evaluate the Catmull-Rom curve's y at a given column.
 * xs are evenly spaced so the bezier's x is linear in t -> exact match
 * with the stroke path used by curve_append. */
static int32_t curve_y_at(int32_t s, int32_t w, const int32_t *xs, const int32_t *ys, int n)
{
    if(w <= 1) return ys[0];
    float fx = (float)s / (float)(w - 1) * (float)(n - 1);
    int i = (int)fx;
    if(i < 0) i = 0;
    if(i > n - 2) i = n - 2;
    float t = fx - i;
    if(t < 0.0f) t = 0.0f;
    if(t > 1.0f) t = 1.0f;
    lv_fpoint_t c1, c2;
    catmull_ctrl(xs, ys, n, i, &c1, &c2);
    float mt = 1.0f - t;
    float y = mt * mt * mt * (float)ys[i]
              + 3.0f * mt * mt * t * c1.y
              + 3.0f * mt * t * t * c2.y
              + t * t * t * (float)ys[i + 1];
    return (int32_t)(y + 0.5f);
}

/* append the smooth curve through points 0..n-1 (move_to first point, then cubic_to) */
static void curve_append(lv_vector_path_t *path, const int32_t *xs, const int32_t *ys, int n)
{
    lv_fpoint_t p, c1, c2;
    p.x = (float)xs[0]; p.y = (float)ys[0];
    lv_vector_path_move_to(path, &p);
    for(int i = 1; i < n; i++) {
        catmull_ctrl(xs, ys, n, i - 1, &c1, &c2);
        p.x = (float)xs[i]; p.y = (float)ys[i];
        lv_vector_path_cubic_to(path, &c1, &c2, &p);
    }
}

static void area_draw_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) return;

    const area_ctx_t *ctx = (const area_ctx_t *)lv_event_get_user_data(e);
    if(!ctx || !ctx->data || ctx->n < 2 || ctx->n > 64) return;
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    int32_t w = lv_area_get_width(&coords);
    int32_t h = lv_area_get_height(&coords);
    if(w <= 0 || h <= 0) return;
    int32_t x0 = coords.x1;
    int32_t y0 = coords.y1;

    int32_t mn = ctx->data[0], mx = ctx->data[0];
    for(uint32_t i = 1; i < ctx->n; i++) {
        if(ctx->data[i] < mn) mn = ctx->data[i];
        if(ctx->data[i] > mx) mx = ctx->data[i];
    }
    int32_t span = mx - mn;
    if(span < 1) span = 1;
    int32_t pad = span / 4 + 1;
    mn -= pad;
    mx += pad;

    int32_t xs[64], ys[64];
    for(uint32_t i = 0; i < ctx->n; i++) {
        xs[i] = x0 + (int32_t)((int64_t)w * (int64_t)i / (int64_t)(ctx->n - 1));
        ys[i] = lv_map(ctx->data[i], mn, mx, y0 + h, y0);
    }


    lv_draw_vector_dsc_t *dsc = lv_draw_vector_dsc_create(layer);
    if(!dsc) return;
    lv_vector_path_t *path = lv_vector_path_create(LV_VECTOR_PATH_QUALITY_MEDIUM);
    if(!path) { lv_draw_vector_dsc_delete(dsc); return; }

    lv_fpoint_t p, c1, c2;


    /* ---- area fill: one native vertical-gradient rect (true smooth gradient),
     *      then erase the part above the curve with per-column background rects. ---- */
    lv_draw_rect_dsc_t gd;
    lv_draw_rect_dsc_init(&gd);
    gd.base.layer = layer;
    gd.bg_color = ctx->color;
    gd.bg_grad.dir = LV_GRAD_DIR_VER;
    gd.bg_grad.stops_count = 2;
    gd.bg_grad.stops[0].color = ctx->color; gd.bg_grad.stops[0].opa = 190; gd.bg_grad.stops[0].frac = 0;
    gd.bg_grad.stops[1].color = ctx->color; gd.bg_grad.stops[1].opa = 0;   gd.bg_grad.stops[1].frac = 255;
    gd.bg_opa = LV_OPA_COVER;
    lv_area_t full;
    full.x1 = x0; full.y1 = y0; full.x2 = x0 + w - 1; full.y2 = y0 + h - 1;
    lv_draw_rect(layer, &gd, &full);

    lv_draw_rect_dsc_t ed;
    lv_draw_rect_dsc_init(&ed);
    ed.base.layer = layer;
    ed.bg_color = ctx->bg;
    ed.bg_opa = LV_OPA_COVER;
    lv_draw_rect_dsc_t gdline;
    lv_draw_rect_dsc_init(&gdline);
    gdline.base.layer = layer;
    gdline.bg_color = ctx->grid_color;
    gdline.bg_opa = LV_OPA_COVER;
    /* erase this many px less per column so the fill tucks under the stroke
     * and the 2px line covers the boundary without a seam */
    const int32_t ERASE_REDUCE = 1;
    for(int32_t s = 0; s < w; s++) {
        int32_t ytop = curve_y_at(s, w, xs, ys, (int)ctx->n);
        if(ytop < y0) ytop = y0;
        if(ytop > y0 + h) ytop = y0 + h;
        int32_t erase_bot = ytop - 1 - ERASE_REDUCE;
        if(erase_bot < y0) continue;
        lv_area_t a;
        a.x1 = x0 + s; a.x2 = x0 + s;
        a.y1 = y0; a.y2 = erase_bot;
        lv_draw_rect(layer, &ed, &a);
        /* draw the gridlines at the same time, wherever the erased area covers them */
        if(ctx->grid > 0) {
            for(uint8_t k = 1; k <= ctx->grid; k++) {
                int32_t gy = y0 + (int32_t)((int64_t)h * (int64_t)k / (int64_t)(ctx->grid + 1));
                if(gy < y0 || gy >= ytop - ERASE_REDUCE) continue;
                lv_area_t g;
                g.x1 = x0 + s; g.x2 = x0 + s;
                g.y1 = gy; g.y2 = gy;
                lv_draw_rect(layer, &gdline, &g);
            }
        }
    }


    /* ---- smooth stroke line on top ---- */
    lv_vector_path_clear(path);
    curve_append(path, xs, ys, (int)ctx->n);
    lv_draw_vector_dsc_set_fill_opa(dsc, 0);
    lv_draw_vector_dsc_set_stroke_color(dsc, ctx->color);
    lv_draw_vector_dsc_set_stroke_opa(dsc, LV_OPA_COVER);
    lv_draw_vector_dsc_set_stroke_width(dsc, 2.0f);
    lv_draw_vector_dsc_add_path(dsc, path);

    lv_draw_vector(dsc);
    lv_vector_path_delete(path);
    lv_draw_vector_dsc_delete(dsc);
}
static void add_gradient_area(lv_obj_t *parent, const int32_t *data, uint32_t n,
                              lv_color_t color, uint8_t grid, lv_color_t grid_color,
                              lv_color_t bg)
{
    if(n < 2) return;
    area_ctx_t *ctx = (area_ctx_t *)lv_malloc(sizeof(area_ctx_t));
    if(!ctx) return;
    int32_t *data_copy = (int32_t *)lv_malloc(n * sizeof(int32_t));
    if(!data_copy) { lv_free(ctx); return; }
    lv_memcpy(data_copy, data, n * sizeof(int32_t));
    ctx->data = data_copy;
    ctx->n = n;
    ctx->color = color;
    ctx->bg = bg;
    ctx->grid = grid;
    ctx->grid_color = grid_color;

    lv_obj_t *area = lv_obj_create(parent);
    lv_obj_remove_style_all(area);
#if LVGL_SIM_AXIS_OVERLAY
    /* Overlay mode: keep the chart at its original full width. */
    lv_obj_set_width(area, LV_PCT(100));
    lv_obj_set_height(area, LV_PCT(100));
#else
    /* Side-axis mode: let flex-grow consume the width left after the labels. */
    lv_obj_set_width(area, LV_SIZE_CONTENT);
    lv_obj_set_height(area, LV_PCT(100));
    lv_obj_set_flex_grow(area, 1);
#endif
    lv_obj_add_event_cb(area, area_draw_cb, LV_EVENT_DRAW_MAIN, ctx);
    lv_obj_add_event_cb(area, area_free_cb, LV_EVENT_DELETE, ctx);
}
void ui_create(void)
{
    gen_data();

    int sum = 0, mx = co2_data[0], mn = co2_data[0];
    for(int i = 0; i < POINTS; i++) {
        sum += co2_data[i];
        if(co2_data[i] > mx) mx = co2_data[i];
        if(co2_data[i] < mn) mn = co2_data[i];
    }
    int avg = sum / POINTS;
    int cur = co2_data[POINTS - 1];

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);

    /* ============ main card ============ */
    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 300, 220);
    lv_obj_set_pos(card, 10, 10);
    set_bg(card, COL_CARD);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_shadow_width(card, 12, 0);
    lv_obj_set_style_shadow_offset_y(card, 4, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 6, 0);

    /* --- title row --- */
    lv_obj_t *trow = lv_obj_create(card);
    lv_obj_remove_style_all(trow);
    lv_obj_set_width(trow, LV_PCT(100));
    lv_obj_set_height(trow, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(trow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(trow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *lgroup = lv_obj_create(trow);
    lv_obj_remove_style_all(lgroup);
    lv_obj_set_size(lgroup, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(lgroup, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(lgroup, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(lgroup, 8, 0);

    /* icon: rounded square with a wind-cross (mirrors reference svg) */
    lv_obj_t *icon = lv_obj_create(lgroup);
    lv_obj_remove_style_all(icon);
    lv_obj_set_size(icon, 28, 28);
    set_bg(icon, COL_ICON_BG);
    lv_obj_set_style_radius(icon, 8, 0);
    lv_obj_t *circle = lv_obj_create(icon);
    lv_obj_remove_style_all(circle);
    lv_obj_set_size(circle, 12, 12);
    lv_obj_set_pos(circle, 8, 8);
    lv_obj_set_style_bg_opa(circle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(circle, 2, 0);
    lv_obj_set_style_border_color(circle, COL_MUTED, 0);
    lv_obj_set_style_radius(circle, 6, 0);
    lv_obj_t *hline = lv_obj_create(icon);
    lv_obj_remove_style_all(hline);
    lv_obj_set_size(hline, 10, 2);
    lv_obj_set_pos(hline, 9, 13);
    set_bg(hline, COL_MUTED);
    lv_obj_t *vline = lv_obj_create(icon);
    lv_obj_remove_style_all(vline);
    lv_obj_set_size(vline, 2, 10);
    lv_obj_set_pos(vline, 13, 9);
    set_bg(vline, COL_MUTED);

    lv_obj_t *tcol = lv_obj_create(lgroup);
    lv_obj_remove_style_all(tcol);
    lv_obj_set_size(tcol, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(tcol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tcol, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_top(tcol, 1, 0);
    mk_label(tcol, "CO2 LEVEL", &lv_font_montserrat_14, COL_TEXT);
    mk_label(tcol, "24H TREND", &lv_font_montserrat_10, COL_FAINT);

    /* status pill (title row, right) */
    lv_obj_t *spill = mk_pill(trow);
    lv_obj_t *sdot = mk_pill_dot(spill);
    lv_obj_t *slbl = mk_label(spill, "NORMAL", &lv_font_montserrat_12, COL_GREEN_TX);

    /* --- value row --- */
    lv_obj_t *vrow = lv_obj_create(card);
    lv_obj_remove_style_all(vrow);
    lv_obj_set_width(vrow, LV_PCT(100));
    lv_obj_set_height(vrow, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(vrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(vrow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(vrow, 6, 0);
    lv_obj_set_style_pad_top(vrow, 2, 0);

    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%d", cur);
    lv_obj_t *val = mk_label(vrow, buf, &lv_font_montserrat_32, COL_TEXT);
    lv_obj_t *unit = mk_label(vrow, "PPM", &lv_font_montserrat_14, COL_MUTED);

    /* --- chart: gradient-filled area --- */
    add_gradient_area(card, co2_data, POINTS, COL_CHART, 3, COL_GRID, COL_CARD);

    /* --- footer row --- */
    lv_obj_t *frow = lv_obj_create(card);
    lv_obj_remove_style_all(frow);
    lv_obj_set_width(frow, LV_PCT(100));
    lv_obj_set_height(frow, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(frow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(frow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_snprintf(buf, sizeof(buf), "AVG %d PPM", avg);
    mk_label(frow, buf, &lv_font_montserrat_10, COL_FAINT);
    lv_snprintf(buf, sizeof(buf), "MIN %d PPM", mn);
    mk_label(frow, buf, &lv_font_montserrat_10, COL_FAINT);
    lv_snprintf(buf, sizeof(buf), "MAX %d PPM", mx);
    mk_label(frow, buf, &lv_font_montserrat_10, COL_FAINT);

    /* status based on current value */
    if(cur >= 1000)      set_pill(spill, sdot, slbl, "HIGH",    COL_RED_BG,   COL_RED,   COL_RED_TX);
    else if(cur >= 800)  set_pill(spill, sdot, slbl, "ELEVATED", COL_AMBER_BG, COL_AMBER, COL_AMBER_TX);
    else                 set_pill(spill, sdot, slbl, "NORMAL",   COL_GREEN_BG, COL_GREEN, COL_GREEN_TX);
}
/* ================== Power meter page ================== */

#define TREND_PTS 28

typedef struct {
    const char *label;
    const char *unit;
    const char *sym;
    lv_color_t accent;
    lv_color_t tint;
    double value;
    double amp;     /* noise amplitude for the trend */
    double drift;   /* per-step drift (e.g. accumulating energy) */
    unsigned seed;
} meter_t;

static void fmt_measure(double v, char *out, size_t n)
{
    char sign = '+';
    if(v < 0) { sign = '-'; v = -v; }
    int whole = (int)v;
    int dec = (int)((v - whole) * 10.0 + 0.5);
    if(dec >= 10) { dec = 0; whole++; }
    if(whole > 999) whole = 999;
    lv_snprintf(out, n, "%c%03d.%d", sign, whole, dec);
}

static void fill_trend(const meter_t *m, int32_t *out)
{
    double v = m->value;
    unsigned s = m->seed;
    for(int i = 0; i < TREND_PTS; i++) {
        double target = m->value + m->drift * i;
        v += (target - v) * 0.15;   /* ease back toward the displayed value */
        s = s * 1664525u + 1013904223u;
        double r = (((s >> 8) % 2001) / 1000.0) - 1.0;   /* -1..1 */
        v += r * m->amp * 0.08;
        out[i] = (int32_t)(v * 10.0 + (v < 0 ? -0.5 : 0.5));
    }
}

static void fmt_axis_value(int32_t raw, char *out, size_t n)
{
    const char sign = raw < 0 ? '-' : ' ';
    const int32_t value = raw < 0 ? -raw : raw;
    lv_snprintf(out, n, "%c%d.%d", sign, (int)(value / 10), (int)(value % 10));
}

static void add_axis_labels(lv_obj_t *parent, const int32_t *data, uint32_t n)
{
    int32_t mn = data[0];
    int32_t mx = data[0];
    for(uint32_t i = 1; i < n; i++) {
        if(data[i] < mn) mn = data[i];
        if(data[i] > mx) mx = data[i];
    }

    lv_obj_t *axis = lv_obj_create(parent);
    lv_obj_remove_style_all(axis);
#if LVGL_SIM_AXIS_OVERLAY
    lv_obj_add_flag(axis, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_pos(axis, 0, 0);
    lv_obj_set_width(axis, LV_PCT(100));
    lv_obj_set_height(axis, LV_PCT(100));
#else
    lv_obj_set_width(axis, 33);
    lv_obj_set_height(axis, LV_PCT(100));
#endif
    lv_obj_set_flex_flow(axis, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(axis, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);

    char max_buf[16], min_buf[16];
    fmt_axis_value(mx, max_buf, sizeof(max_buf));
    fmt_axis_value(mn, min_buf, sizeof(min_buf));
    lv_obj_t *max_label = lv_label_create(axis);
    lv_label_set_text(max_label, max_buf);
    lv_obj_set_width(max_label, LV_PCT(100));
    lv_obj_set_style_text_align(max_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(max_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(max_label, COL_MUTED, 0);

    lv_obj_t *min_label = lv_label_create(axis);
    lv_label_set_text(min_label, min_buf);
    lv_obj_set_width(min_label, LV_PCT(100));
    lv_obj_set_style_text_align(min_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(min_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(min_label, COL_MUTED, 0);
}

static void add_sparkline(lv_obj_t *card, const meter_t *m, const int32_t *data)
{
    lv_obj_t *chart_row = lv_obj_create(card);
    lv_obj_remove_style_all(chart_row);
    lv_obj_set_width(chart_row, LV_PCT(100));
    lv_obj_set_flex_grow(chart_row, 1);
    lv_obj_set_flex_flow(chart_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chart_row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    add_gradient_area(chart_row, data, TREND_PTS, m->accent, 0, COL_GRID, COL_CARD);
    add_axis_labels(chart_row, data, TREND_PTS);
}

static void make_meter_card(lv_obj_t *parent, int x, int y, const meter_t *m)
{
    char buf[16];
    int32_t trend[TREND_PTS];
    fill_trend(m, trend);

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, 148, 112);
    set_bg(card, COL_CARD);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_offset_y(card, 3, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 4, 0);

    /* label row */
    lv_obj_t *lrow = lv_obj_create(card);
    lv_obj_remove_style_all(lrow);
    lv_obj_set_size(lrow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(lrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(lrow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(lrow, 6, 0);

    lv_obj_t *icon = lv_obj_create(lrow);
    lv_obj_remove_style_all(icon);
    lv_obj_set_size(icon, 20, 20);
    set_bg(icon, m->tint);
    lv_obj_set_style_radius(icon, 6, 0);
    lv_obj_set_flex_flow(icon, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(icon, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *isym = lv_label_create(icon);
    lv_label_set_text(isym, m->sym);
    lv_obj_set_style_text_font(isym, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(isym, m->accent, 0);

    lv_obj_t *lbl = lv_label_create(lrow);
    lv_label_set_text(lbl, m->label);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl, COL_MUTED, 0);

    /* value row - right aligned so the decimal point stays put when digits change */
    lv_obj_t *vrow = lv_obj_create(card);
    lv_obj_remove_style_all(vrow);
    lv_obj_set_size(vrow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(vrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(vrow, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(vrow, 4, 0);

    fmt_measure(m->value, buf, sizeof(buf));
    lv_obj_t *val = lv_label_create(vrow);
    lv_label_set_text(val, buf);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(val, COL_TEXT, 0);

    lv_obj_t *unit = lv_label_create(vrow);
    lv_label_set_text(unit, m->unit);
    lv_obj_set_style_text_font(unit, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(unit, COL_MUTED, 0);
    lv_obj_set_style_pad_bottom(unit, 3, 0);

    /* sparkline trend */
    add_sparkline(card, m, trend);
}

lv_obj_t *ui_create_power(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, COL_BG, 0);

    const meter_t meters[4] = {
        { "VOLTAGE", "V",   "V",   lv_color_hex(0x3B82F6), lv_color_hex(0xDBEAFE), 220.5, 1.5, 0.00, 1001u },
        { "CURRENT", "A",   "A",   lv_color_hex(0x10B981), lv_color_hex(0xD1FAE5), -12.3, 4.0, 0.00, 2002u },
        { "POWER",   "W",   "W",   lv_color_hex(0xF59E0B), lv_color_hex(0xFEF3C7), -456.7, 80.0, 0.00, 3003u },
        { "ENERGY",  "kWh", "E",   lv_color_hex(0x8B5CF6), lv_color_hex(0xEDE9FE), 123.4, 0.4, 0.30, 4004u },
    };
    const int pos[4][2] = { {8, 4}, {164, 4}, {8, 124}, {164, 124} };
    for(int i = 0; i < 4; i++) make_meter_card(scr, pos[i][0], pos[i][1], &meters[i]);
    return scr;
}