# LVGL 320x240 Simulator (CO2 + Power dashboards)

LVGL v9.5.0 + SDL2 simulator running at 320x240 on the sandbox (192.168.114.138).

## Pages
- Page 1 - CO2 dashboard: value + 24h smooth gradient area trend chart
- Page 2 - Power meter: VOLTAGE / CURRENT / POWER / ENERGY cards, each with a
  right-aligned `±XXX.X` value and a gradient area sparkline

## Layout
- `lv_conf.h`    LVGL config (32bpp, 1MB mem pool, Montserrat fonts, thorvg vector)
- `CMakeLists.txt`
- `src/main.c`   entry: 320x240 display, SDL window (2x), `--page=1|2 --screenshot=out.png`, SPACE switches pages
- `src/ui.c`     both pages
- `src/png.c`    pure-C + zlib PNG writer

## Notes
- LVGL sources (`lvgl/`) are gitignored; it's v9.5.0 cloned from the gitee mirror.
- The trend area is filled via solid 1px-wide convex rect strips grouped into
  24 horizontal bands (thorvg's gradient fill of concave paths is unreliable -
  it fills the bounding box). A cleaner `lv_draw_rect` gradient experiment
  lives on the `try/rect-gradient-erase` branch.