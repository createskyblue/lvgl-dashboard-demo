# LVGL 320×240 模拟器

LVGL v9.5.0 + SDL2 · 双页面数据仪表盘

## 页面

**CO₂ 数据仪表盘** — 浓度 + 24h 平滑渐变趋势图

![CO₂ 数据仪表盘](docs/co2_dashboard.png)

**电力监控** — 电压 / 电流 / 功率 / 电量，`±XXX.X` 右对齐 + 渐变趋势图

![电力监控](docs/power_dashboard.png)

## 渐变面积图思路（src/ui.c）

1. 整块 `lv_draw_rect` 垂直渐变矩形 → 真·平滑渐变
2. 逐列擦除曲线上方（擦除边界按 Catmull-Rom 贝塞尔逐列求值，与描线一致）
3. 擦除时同步画网格线（仅白色区）
4. 每列少擦 1px → 填充藏在描线下方，无接缝

> 不用 thorvg 渐变填充凹多边形：thorvg 会按包围盒填充，导致颜色溢出曲线外。

## 运行

```bash
./build/lvgl_sim                     # 交互模式（空格切页）
./build/lvgl_sim --page=1 --screenshot=co2.png
./build/lvgl_sim --page=2 --screenshot=power.png
```

## 结构

`src/main.c` 入口 · `src/ui.c` 页面 UI · `src/png.c` PNG 写出 · `lv_conf.h` 配置