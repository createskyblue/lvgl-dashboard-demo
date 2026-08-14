#!/bin/bash
cd ~/lvgl_sim/lvgl
echo "== gradient mapping in sw renderer =="
grep -rn -B3 -A20 "LV_GRAD_TYPE_LINEAR\|linear.start\|grad->start" src/draw/sw/lv_draw_sw_rect.c | head -60
echo "== lv_gradient_get_color_stop / gradient area calc =="
grep -rn "grad->dir == LV_GRAD_DIR_VER\|grad->dir == LV_GRAD_DIR_HOR\|LV_GRAD_TYPE_LINEAR" src/draw/sw/lv_draw_sw_rect.c | head