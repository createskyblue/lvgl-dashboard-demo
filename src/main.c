#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL.h>
#include "lvgl.h"
#include "ui.h"
#include "png.h"

#define HOR_RES 320
#define VER_RES 240
#define SCALE 2

static lv_display_t *disp;
static lv_color_t *fbuf;
static int g_stride = 0;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static int sdl_active = 0;

static uint32_t tick_cb(void)
{
    if(sdl_active) return SDL_GetTicks();
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    LV_UNUSED(a);
    if(sdl_active) {
        SDL_UpdateTexture(texture, NULL, px, HOR_RES * 4);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }
    lv_display_flush_ready(d);
}

static void save_screenshot(const char *path)
{
    lv_refr_now(disp);
    int rc = png_save(path, (const uint8_t *)fbuf, HOR_RES, VER_RES, g_stride);
    printf("%s -> %s (%dx%d)\n", rc == 0 ? "saved" : "FAILED", path, HOR_RES, VER_RES);
}

int main(int argc, char **argv)
{
    const char *shot = NULL;
    int page = 1;
    for(int i = 1; i < argc; i++) {
        if(strncmp(argv[i], "--screenshot=", 13) == 0) shot = argv[i] + 13;
        else if(strncmp(argv[i], "--page=", 7) == 0) page = atoi(argv[i] + 7);
    }

    if(!shot) {
        if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
            fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
            return 1;
        }
        window = SDL_CreateWindow("LVGL 320x240 Simulator",
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  HOR_RES * SCALE, VER_RES * SCALE, SDL_WINDOW_SHOWN);
        if(!window) {
            fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
            return 1;
        }
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if(!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, HOR_RES, VER_RES);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        sdl_active = 1;
    }

    lv_init();
    lv_tick_set_cb(tick_cb);

    disp = lv_display_create(HOR_RES, VER_RES);
    {
        lv_color_format_t cf = lv_display_get_color_format(disp);
        uint32_t stride = lv_draw_buf_width_to_stride(HOR_RES, cf);
        g_stride = (int)stride;
        uint32_t buf_size = stride * VER_RES;
        fbuf = (lv_color_t *)malloc(buf_size);
        if(!fbuf) { fprintf(stderr, "OOM\n"); return 1; }
        lv_display_set_buffers(disp, fbuf, NULL, buf_size, LV_DISPLAY_RENDER_MODE_FULL);
    }
    lv_display_set_flush_cb(disp, flush_cb);

    ui_create();                      /* page 1: CO2 dashboard */
    lv_obj_t *scr1 = lv_screen_active();
    lv_obj_t *power_scr = ui_create_power();   /* page 2: power meter */
    lv_screen_load(scr1);

    if(shot) {
        if(page == 2) lv_screen_load(power_scr);
        for(int i = 0; i < 8; i++) lv_timer_handler();
        save_screenshot(shot);
        free(fbuf);
        lv_deinit();
        return 0;
    }

    uint32_t next = SDL_GetTicks();
    for(;;) {
        SDL_Event e;
        while(SDL_PollEvent(&e)) {
            if(e.type == SDL_QUIT) goto done;
            if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_SPACE) {
                if(lv_screen_active() == power_scr) lv_screen_load(scr1);
                else lv_screen_load(power_scr);
            }
        }
        lv_timer_handler();
        uint32_t now = SDL_GetTicks();
        if(now < next) SDL_Delay(next - now);
        next = now + 16;
    }

done:
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(fbuf);
    lv_deinit();
    return 0;
}