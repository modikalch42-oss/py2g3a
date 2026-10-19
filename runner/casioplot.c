// casioplot and time modules for the py2g3a runner.
//
// casioplot follows Casio's own module (the one in the calculator's built-in
// Python): a 384x192 drawing area under the status bar, drawn off-screen
// until show_screen(). Extras that Casio's fx-CG50 version doesn't have:
//   fill_rect(x, y, w, h, color)   fast filled rectangle
//   getkey()                       non-blocking key read (0 = no key)
//   KEY_* constants                codes returned by getkey()

#include <stdint.h>
#include <string.h>

#ifndef NO_QSTR   // MicroPython's qstr scan runs on the host, without these headers
#include <fxcg/display.h>
#include <fxcg/keyboard.h>
#include <fxcg/rtc.h>
#endif

#include "py/runtime.h"
#include "py/obj.h"
#include "font_data.h"

#define PLOT_W 384
#define PLOT_H 192
#define PLOT_Y0 24      // the status bar occupies the top 24 rows of VRAM

int casioplot_active;   // read by main.c: when set, the text console stays hidden

void casioplot_reset(void) {
    casioplot_active = 0;
}

static uint16_t *vram(void) {
    return (uint16_t *)GetVRAMAddress();
}

static void enter_graphics(void) {
    if (!casioplot_active) {
        casioplot_active = 1;
        uint16_t *v = vram();
        for (int i = PLOT_Y0 * PLOT_W; i < (PLOT_Y0 + PLOT_H) * PLOT_W; i++) {
            v[i] = 0xFFFF;
        }
        DisplayStatusArea();
    }
}

// Colours are (r, g, b) tuples with 0-255 components, like Casio's module.
static uint16_t to565(mp_obj_t color) {
    if (mp_obj_is_int(color)) {
        return (uint16_t)mp_obj_get_int(color);
    }
    size_t n;
    mp_obj_t *items;
    mp_obj_get_array(color, &n, &items);
    if (n != 3) {
        mp_raise_ValueError(MP_ERROR_TEXT("colour must be (r, g, b)"));
    }
    int r = mp_obj_get_int(items[0]), g = mp_obj_get_int(items[1]), b = mp_obj_get_int(items[2]);
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static uint16_t arg_color(size_t n_args, const mp_obj_t *args, size_t i) {
    return n_args > i ? to565(args[i]) : 0x0000;
}

static inline void put(int x, int y, uint16_t c) {
    if ((unsigned)x < PLOT_W && (unsigned)y < PLOT_H) {
        vram()[(y + PLOT_Y0) * PLOT_W + x] = c;
    }
}

static mp_obj_t cp_clear_screen(void) {
    casioplot_active = 0;
    enter_graphics();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(cp_clear_screen_obj, cp_clear_screen);

static mp_obj_t cp_show_screen(void) {
    enter_graphics();
    Bdisp_PutDisp_DD();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(cp_show_screen_obj, cp_show_screen);

static mp_obj_t cp_set_pixel(size_t n_args, const mp_obj_t *args) {
    enter_graphics();
    put(mp_obj_get_int(args[0]), mp_obj_get_int(args[1]), arg_color(n_args, args, 2));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(cp_set_pixel_obj, 2, 3, cp_set_pixel);

static mp_obj_t cp_get_pixel(mp_obj_t x_in, mp_obj_t y_in) {
    enter_graphics();
    int x = mp_obj_get_int(x_in), y = mp_obj_get_int(y_in);
    if ((unsigned)x >= PLOT_W || (unsigned)y >= PLOT_H) {
        return mp_const_none;
    }
    uint16_t c = vram()[(y + PLOT_Y0) * PLOT_W + x];
    mp_obj_t t[3] = {
        MP_OBJ_NEW_SMALL_INT((c >> 11) << 3),
        MP_OBJ_NEW_SMALL_INT(((c >> 5) & 0x3F) << 2),
        MP_OBJ_NEW_SMALL_INT((c & 0x1F) << 3),
    };
    return mp_obj_new_tuple(3, t);
}
static MP_DEFINE_CONST_FUN_OBJ_2(cp_get_pixel_obj, cp_get_pixel);

static mp_obj_t cp_fill_rect(size_t n_args, const mp_obj_t *args) {
    enter_graphics();
    int x = mp_obj_get_int(args[0]), y = mp_obj_get_int(args[1]);
    int w = mp_obj_get_int(args[2]), h = mp_obj_get_int(args[3]);
    uint16_t c = arg_color(n_args, args, 4);
    if (w < 0) { x += w; w = -w; }
    if (h < 0) { y += h; h = -h; }
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > PLOT_W ? PLOT_W : x + w, y1 = y + h > PLOT_H ? PLOT_H : y + h;
    uint16_t *v = vram();
    for (int yy = y0; yy < y1; yy++) {
        uint16_t *row = v + (yy + PLOT_Y0) * PLOT_W;
        for (int xx = x0; xx < x1; xx++) {
            row[xx] = c;
        }
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(cp_fill_rect_obj, 4, 5, cp_fill_rect);

static mp_obj_t cp_draw_string(size_t n_args, const mp_obj_t *args) {
    enter_graphics();
    int x = mp_obj_get_int(args[0]), y = mp_obj_get_int(args[1]);
    const char *s = mp_obj_str_get_str(args[2]);
    uint16_t c = arg_color(n_args, args, 3);

    const uint32_t *rows = font_medium_rows;
    const uint16_t *offs = font_medium_offs;
    const uint8_t *widths = font_medium_widths;
    int h = FONT_MEDIUM_HEIGHT;
    if (n_args > 4) {
        const char *size = mp_obj_str_get_str(args[4]);
        if (!strcmp(size, "small")) {
            rows = font_small_rows; offs = font_small_offs; widths = font_small_widths; h = FONT_SMALL_HEIGHT;
        } else if (!strcmp(size, "large")) {
            rows = font_large_rows; offs = font_large_offs; widths = font_large_widths; h = FONT_LARGE_HEIGHT;
        } else if (strcmp(size, "medium")) {
            mp_raise_ValueError(MP_ERROR_TEXT("size must be 'small', 'medium' or 'large'"));
        }
    }
    for (; *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch >= 0x80) {
            if (ch < 0xC0) {
                continue;            // UTF-8 continuation byte
            }
            ch = '?';
        }
        if (ch < 32 || ch > 126) {
            continue;
        }
        int g = ch - 32;
        const uint32_t *gr = rows + offs[g];
        for (int yy = 0; yy < h; yy++) {
            uint32_t bits = gr[yy];
            for (int xx = 0; bits; xx++, bits <<= 1) {
                if (bits & 0x80000000u) {
                    put(x + xx, y + yy, c);
                }
            }
        }
        x += widths[g];
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(cp_draw_string_obj, 3, 5, cp_draw_string);

static mp_obj_t cp_getkey(void) {
    return MP_OBJ_NEW_SMALL_INT(PRGM_GetKey());
}
static MP_DEFINE_CONST_FUN_OBJ_0(cp_getkey_obj, cp_getkey);

static const mp_rom_map_elem_t casioplot_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_casioplot) },
    { MP_ROM_QSTR(MP_QSTR_clear_screen), MP_ROM_PTR(&cp_clear_screen_obj) },
    { MP_ROM_QSTR(MP_QSTR_show_screen), MP_ROM_PTR(&cp_show_screen_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_pixel), MP_ROM_PTR(&cp_set_pixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_pixel), MP_ROM_PTR(&cp_get_pixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_string), MP_ROM_PTR(&cp_draw_string_obj) },
    { MP_ROM_QSTR(MP_QSTR_fill_rect), MP_ROM_PTR(&cp_fill_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_getkey), MP_ROM_PTR(&cp_getkey_obj) },
    { MP_ROM_QSTR(MP_QSTR_KEY_LEFT), MP_ROM_INT(KEY_PRGM_LEFT) },
    { MP_ROM_QSTR(MP_QSTR_KEY_RIGHT), MP_ROM_INT(KEY_PRGM_RIGHT) },
    { MP_ROM_QSTR(MP_QSTR_KEY_UP), MP_ROM_INT(KEY_PRGM_UP) },
    { MP_ROM_QSTR(MP_QSTR_KEY_DOWN), MP_ROM_INT(KEY_PRGM_DOWN) },
    { MP_ROM_QSTR(MP_QSTR_KEY_EXE), MP_ROM_INT(KEY_PRGM_RETURN) },
    { MP_ROM_QSTR(MP_QSTR_KEY_EXIT), MP_ROM_INT(KEY_PRGM_EXIT) },
    { MP_ROM_QSTR(MP_QSTR_KEY_SHIFT), MP_ROM_INT(KEY_PRGM_SHIFT) },
    { MP_ROM_QSTR(MP_QSTR_KEY_ALPHA), MP_ROM_INT(KEY_PRGM_ALPHA) },
    { MP_ROM_QSTR(MP_QSTR_KEY_OPTN), MP_ROM_INT(KEY_PRGM_OPTN) },
    { MP_ROM_QSTR(MP_QSTR_KEY_F1), MP_ROM_INT(KEY_PRGM_F1) },
    { MP_ROM_QSTR(MP_QSTR_KEY_F2), MP_ROM_INT(KEY_PRGM_F2) },
    { MP_ROM_QSTR(MP_QSTR_KEY_F3), MP_ROM_INT(KEY_PRGM_F3) },
    { MP_ROM_QSTR(MP_QSTR_KEY_F4), MP_ROM_INT(KEY_PRGM_F4) },
    { MP_ROM_QSTR(MP_QSTR_KEY_F5), MP_ROM_INT(KEY_PRGM_F5) },
    { MP_ROM_QSTR(MP_QSTR_KEY_F6), MP_ROM_INT(KEY_PRGM_F6) },
    { MP_ROM_QSTR(MP_QSTR_KEY_0), MP_ROM_INT(KEY_PRGM_0) },
    { MP_ROM_QSTR(MP_QSTR_KEY_1), MP_ROM_INT(KEY_PRGM_1) },
    { MP_ROM_QSTR(MP_QSTR_KEY_2), MP_ROM_INT(KEY_PRGM_2) },
    { MP_ROM_QSTR(MP_QSTR_KEY_3), MP_ROM_INT(KEY_PRGM_3) },
    { MP_ROM_QSTR(MP_QSTR_KEY_4), MP_ROM_INT(KEY_PRGM_4) },
    { MP_ROM_QSTR(MP_QSTR_KEY_5), MP_ROM_INT(KEY_PRGM_5) },
    { MP_ROM_QSTR(MP_QSTR_KEY_6), MP_ROM_INT(KEY_PRGM_6) },
    { MP_ROM_QSTR(MP_QSTR_KEY_7), MP_ROM_INT(KEY_PRGM_7) },
    { MP_ROM_QSTR(MP_QSTR_KEY_8), MP_ROM_INT(KEY_PRGM_8) },
    { MP_ROM_QSTR(MP_QSTR_KEY_9), MP_ROM_INT(KEY_PRGM_9) },
};
static MP_DEFINE_CONST_DICT(casioplot_globals, casioplot_globals_table);

const mp_obj_module_t mp_module_casioplot = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&casioplot_globals,
};
MP_REGISTER_MODULE(MP_QSTR_casioplot, mp_module_casioplot);

// --------------------------------------------------------------------------
// time: sleep(), sleep_ms(), ticks_ms(), monotonic()
// The calculator's clock ticks 128 times a second (~8 ms resolution).
// --------------------------------------------------------------------------

static void wait_ticks(int ticks) {
    int start = RTC_GetTicks();
    for (;;) {
        int now = RTC_GetTicks();
        if (now < start) {
            start = now;              // the tick counter resets at midnight
        }
        if (now - start >= ticks) {
            break;
        }
        casio_poll();                 // lets AC/ON stop the script
        mp_handle_pending(true);
    }
}

static mp_obj_t time_sleep(mp_obj_t s) {
    mp_float_t sec = mp_obj_get_float(s);
    if (sec > 0) {
        wait_ticks((int)(sec * 128 + 0.5));
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(time_sleep_obj, time_sleep);

static mp_obj_t time_sleep_ms(mp_obj_t ms) {
    mp_int_t n = mp_obj_get_int(ms);
    if (n > 0) {
        wait_ticks((int)((n * 128 + 500) / 1000));
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(time_sleep_ms_obj, time_sleep_ms);

static mp_obj_t time_ticks_ms(void) {
    return mp_obj_new_int(((mp_int_t)RTC_GetTicks() * 1000) / 128);
}
static MP_DEFINE_CONST_FUN_OBJ_0(time_ticks_ms_obj, time_ticks_ms);

static mp_obj_t time_monotonic(void) {
    return mp_obj_new_float((mp_float_t)RTC_GetTicks() / 128);
}
static MP_DEFINE_CONST_FUN_OBJ_0(time_monotonic_obj, time_monotonic);

static const mp_rom_map_elem_t time_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_time) },
    { MP_ROM_QSTR(MP_QSTR_sleep), MP_ROM_PTR(&time_sleep_obj) },
    { MP_ROM_QSTR(MP_QSTR_sleep_ms), MP_ROM_PTR(&time_sleep_ms_obj) },
    { MP_ROM_QSTR(MP_QSTR_ticks_ms), MP_ROM_PTR(&time_ticks_ms_obj) },
    { MP_ROM_QSTR(MP_QSTR_monotonic), MP_ROM_PTR(&time_monotonic_obj) },
    { MP_ROM_QSTR(MP_QSTR_time), MP_ROM_PTR(&time_monotonic_obj) },
};
static MP_DEFINE_CONST_DICT(time_globals, time_globals_table);

const mp_obj_module_t mp_module_time = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&time_globals,
};
MP_REGISTER_MODULE(MP_QSTR_time, mp_module_time);
