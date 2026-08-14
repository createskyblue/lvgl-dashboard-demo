#!/bin/bash
cd ~/lvgl_sim/lvgl
grep -rln "grad_linear_get_line" src/
echo "== impl =="
grep -rn -A30 "lv_draw_sw_grad_linear_get_line" src/draw/sw/*.c | head -45