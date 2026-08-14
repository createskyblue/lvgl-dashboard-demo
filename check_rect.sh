#!/bin/bash
cd ~/lvgl_sim/lvgl
echo "== draw_rect dsc struct =="
sed -n '/typedef struct {/,/} lv_draw_rect_dsc_t;/p' src/draw/lv_draw_rect.h | head -60
echo "== lv_grad_dsc_t =="
sed -n '/typedef struct {/,/} lv_grad_dsc_t;/p' src/misc/lv_grad.h | head -30