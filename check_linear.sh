#!/bin/bash
cd ~/lvgl_sim/lvgl
echo "== COMPLEX_GRADIENTS default =="
grep -n -A4 "LV_USE_DRAW_SW_COMPLEX_GRADIENTS" src/lv_conf_internal.h | head -8
echo "== grad_linear_get_line impl =="
grep -rn -A25 "void lv_draw_sw_grad_linear_get_line" src/draw/sw/lv_draw_sw_gradient.c | head -40