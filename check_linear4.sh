#!/bin/bash
cd ~/lvgl_sim/lvgl
grep -n "grad_linear_get_line" src/draw/sw/lv_draw_sw_grad.c
grep -rn -A28 "grad_linear_get_line" src/draw/sw/lv_draw_sw_grad.h | head -40