#!/bin/bash
cd ~/lvgl_sim/lvgl
grep -n "grad_linear_setup" src/draw/sw/lv_draw_sw_grad.c
sed -n '/lv_draw_sw_grad_linear_setup/,/^}/p' src/draw/sw/lv_draw_sw_grad.c | head -60