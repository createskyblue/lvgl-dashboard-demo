#!/bin/bash
cd ~/lvgl_sim/lvgl
sed -n '/void lv_draw_sw_grad_linear_get_line/,/^}/p' src/draw/sw/lv_draw_sw_grad.c