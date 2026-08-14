#!/bin/bash
cd ~/lvgl_sim/lvgl
echo "== find rect/gradient draw =="
grep -rln "lv_gradient\|LV_GRAD_TYPE_LINEAR" src/draw/sw/ | head
echo "== how VER/HOR/LINEAR dirs are handled =="
grep -rn -B2 -A12 "LV_GRAD_DIR_VER" src/draw/sw/*.c | head -40