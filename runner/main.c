// py2g3a runner: a tiny MicroPython add-in that runs the script stored in
// its "slot" at the end of the add-in file. py2g3a.py fills the slot.

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <fxcg/display.h>
#include <fxcg/keyboard.h>
#include <fxcg/rtc.h>

#include "py/compile.h"
#include "py/gc.h"
#include "py/runtime.h"
#include "py/stackctrl.h"
#include "py/mphal.h"
#include "shared/readline/readline.h"
#include "shared/runtime/gchelper.h"

// --------------------------------------------------------------------------
// The script slot. The linker script puts this section last in the add-in,
// so py2g3a.py can replace everything from `payload` onwards with a script
// of any length.
// --------------------------------------------------------------------------

struct py2g3a_slot {
    char magic[16];        // "PY2G3A:SLOT:v1"
    uint32_t version;      // 1
    uint32_t length;       // script length in bytes, 0 = empty
    uint32_t crc;          // CRC-32 of the script
    uint32_t reserved;
    char payload[4];       // script text + NUL (grows past the end of the file)
};

__attribute__((section(".py2g3a_slot"), used, aligned(4)))
const struct py2g3a_slot py2g3a_slot = { "PY2G3A:SLOT:v1", 1, 0, 0, 0, { 0 } };

static uint32_t crc32(const uint8_t *p, uint32_t n) {
    uint32_t c = 0xFFFFFFFF;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++) {
            c = (c >> 1) ^ (0xEDB88320 & -(c & 1));
        }
    }
    return ~c;
}

// casioplot.c: while a script is drawing graphics, keep the console hidden.
extern int casioplot_active;
void casioplot_reset(void);

// --------------------------------------------------------------------------
// Text console: 21x8 characters (the OS's PrintXY grid), with scrollback.
// --------------------------------------------------------------------------

#define COLS 21
#define ROWS 8
#define HIST 200

static char lines[HIST][COLS + 1];
static int top;            // ring index of the oldest line
static int count;          // number of lines in use (>= 1)
static int col;            // cursor column on the newest line
static int scroll;         // lines scrolled back from the bottom (viewing only)
static int dirty;
static int last_draw;
static int cursor_on;      // show '_' cursor (during input())
static const char *status_msg = "";

static char *line_at(int i) {
    return lines[(top + i) % HIST];
}

static void console_reset(void) {
    memset(lines, 0, sizeof(lines));
    top = 0;
    count = 1;
    col = 0;
    scroll = 0;
    dirty = 1;
}

static void console_newline(void) {
    if (count < HIST) {
        count++;
    } else {
        top = (top + 1) % HIST;
    }
    memset(line_at(count - 1), 0, COLS + 1);
    col = 0;
    dirty = 1;
}

static void console_draw(void) {
    char buf[COLS + 3];
    Bdisp_AllClr_VRAM();
    DefineStatusMessage((char *)status_msg, 1, 0, 0);
    DisplayStatusArea();
    int first = count - ROWS - scroll;
    if (first < 0) {
        first = 0;
    }
    for (int r = 0; r < ROWS && first + r < count; r++) {
        // PrintXY skips the first two characters of its string.
        buf[0] = buf[1] = ' ';
        strcpy(buf + 2, line_at(first + r));
        if (cursor_on && scroll == 0 && first + r == count - 1 && col < COLS) {
            size_t n = strlen(buf);
            buf[n] = '_';
            buf[n + 1] = 0;
        }
        if (buf[2]) {
            PrintXY(1, r + 1, buf, TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
        }
    }
    Bdisp_PutDisp_DD();
    dirty = 0;
    last_draw = RTC_GetTicks();
}

// Redraw at most every ~60 ms so printing in a loop stays fast.
static void console_maybe_draw(void) {
    if (casioplot_active) {
        return;
    }
    if (dirty && (unsigned)(RTC_GetTicks() - last_draw) >= 8) {
        console_draw();
    }
}

static void console_putc(unsigned char c) {
    scroll = 0;
    if (c == '\n') {
        console_newline();
        console_maybe_draw();
        return;
    }
    if (c == '\r') {
        return;
    }
    if (c == '\t') {
        do {
            console_putc(' ');
        } while (col % 4);
        return;
    }
    if (c >= 0x80) {
        // The menu font can't show non-ASCII; show one '?' per character.
        if (c < 0xC0) {
            return;   // UTF-8 continuation byte
        }
        c = '?';
    } else if (c < 0x20) {
        return;
    }
    if (col >= COLS) {
        console_newline();
    }
    line_at(count - 1)[col++] = c;
    dirty = 1;
}

static void console_write(const char *s, size_t n) {
    while (n--) {
        console_putc((unsigned char)*s++);
    }
}

static void console_backspace(void) {
    if (col == 0 && count > 1) {
        count--;
        col = COLS;
    }
    if (col > 0) {
        line_at(count - 1)[--col] = 0;
    }
    dirty = 1;
}

// --------------------------------------------------------------------------
// MicroPython HAL
// --------------------------------------------------------------------------

void mp_hal_stdout_tx_strn_cooked(const char *str, size_t len) {
    console_write(str, len);
}

mp_uint_t mp_hal_stdout_tx_strn(const char *str, size_t len) {
    console_write(str, len);
    return len;
}

void mp_hal_stdout_tx_str(const char *str) {
    console_write(str, strlen(str));
}

int mp_hal_stdin_rx_chr(void) {
    int key;
    GetKey(&key);
    return key & 0xFF;
}

// micropython.kbd_intr() calls this; the stop keys are fixed, so ignore it.
void mp_hal_set_interrupt_char(int c) {
    (void)c;
}

unsigned int casio_random_seed(void) {
    return (unsigned int)RTC_GetTicks() * 2654435761u;
}

// Called by the VM every few thousand bytecodes.
void casio_poll(void) {
    int k = PRGM_GetKey();
    // EXIT is a normal game key once casioplot is in use; AC/ON always stops.
    if (k == KEY_PRGM_ACON || (k == KEY_PRGM_EXIT && !casioplot_active)) {
        mp_sched_keyboard_interrupt();
    }
    console_maybe_draw();
}

// Translate a GetKey() code into text for input().
static const char *key_text(int key) {
    static char one[2];
    switch (key) {
        case KEY_CHAR_PLUS:   return "+";
        case KEY_CHAR_MINUS:  return "-";
        case KEY_CHAR_PMINUS: return "-";
        case KEY_CHAR_MULT:   return "*";
        case KEY_CHAR_DIV:    return "/";
        case KEY_CHAR_FRAC:   return "/";
        case KEY_CHAR_POW:    return "**";
        case KEY_CHAR_SQUARE: return "**2";
        case KEY_CHAR_EXP:    return "e";
        case KEY_CHAR_EXPN10: return "10**";
        case KEY_CHAR_PI:     return "3.141592653589793";
        case KEY_CHAR_STORE:  return "=";
    }
    if (key >= 0x20 && key < 0x7F) {
        one[0] = (char)key;
        one[1] = 0;
        return one;
    }
    return NULL;
}

int casio_readline(vstr_t *line, const char *prompt) {
    console_write(prompt, strlen(prompt));
    const char *saved = status_msg;
    casioplot_active = 0;   // the text screen replaces any graphics
    status_msg = "Type, then EXE";
    cursor_on = 1;
    for (;;) {
        console_draw();
        int key;
        GetKey(&key);
        if (key == KEY_CTRL_EXE) {
            break;
        }
        if (key == KEY_CTRL_DEL) {
            if (line->len > 0) {
                vstr_cut_tail_bytes(line, 1);
                console_backspace();
            }
            continue;
        }
        if (key == KEY_CTRL_AC || key == KEY_CTRL_EXIT) {
            cursor_on = 0;
            status_msg = saved;
            console_putc('\n');
            return CHAR_CTRL_C;
        }
        const char *t = key_text(key);
        if (t) {
            vstr_add_str(line, t);
            console_write(t, strlen(t));
        }
    }
    cursor_on = 0;
    status_msg = saved;
    console_putc('\n');
    return 0;
}

// --------------------------------------------------------------------------
// Runtime glue normally provided by the embed port
// --------------------------------------------------------------------------

void gc_collect(void) {
    gc_collect_start();
    gc_helper_collect_regs_and_stack();
    gc_collect_end();
}

void nlr_jump_fail(void *val) {
    (void)val;
    console_write("\nFatal error.\n", 14);
    console_draw();
    for (;;) {
        int key;
        GetKey(&key);
    }
}

// --------------------------------------------------------------------------
// main
// --------------------------------------------------------------------------

static void print_str(const char *s) {
    console_write(s, strlen(s));
}

// Returns 1 if the script ended with an error (or was stopped).
static int run_script(const char *src, size_t len, void *heap, size_t heap_size, void *stack_top) {
    int failed = 0;
    mp_stack_set_top(stack_top);
    mp_stack_set_limit(48 * 1024);
    gc_init(heap, (uint8_t *)heap + heap_size);
    mp_init();

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, src, len, 0);
        qstr source_name = lex->source_name;
        mp_parse_tree_t tree = mp_parse(lex, MP_PARSE_FILE_INPUT);
        mp_obj_t fun = mp_compile(&tree, source_name, false);
        mp_call_function_0(fun);
        nlr_pop();
    } else {
        failed = 1;
        mp_obj_t exc = (mp_obj_t)nlr.ret_val;
        if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(mp_obj_get_type(exc)),
                                    MP_OBJ_FROM_PTR(&mp_type_KeyboardInterrupt))) {
            if (col > 0) {
                console_putc('\n');
            }
            print_str("[Stopped]\n");
        } else {
            mp_obj_print_exception(&mp_plat_print, exc);
        }
    }
    mp_deinit();
    return failed;
}

int main(void) {
    int stack_marker;
    int key;

    Bdisp_EnableColor(1);
    console_reset();

#ifdef PY2G3A_HOST_TEST
    extern const struct py2g3a_slot *py2g3a_test_slot;   // test harness only
    const volatile struct py2g3a_slot *slot = py2g3a_test_slot;
#else
    const volatile struct py2g3a_slot *slot = &py2g3a_slot;
#endif
    uint32_t len = slot->length;
    const char *src = (const char *)(uintptr_t)slot->payload;

    if (len == 0) {
        status_msg = "py2g3a runner";
        print_str("No script inside.\n\nUse py2g3a on a\ncomputer to turn a\n.py file into an\nadd-in.\n");
        console_draw();
        for (;;) {
            GetKey(&key);
        }
    }
    if (crc32((const uint8_t *)src, len) != slot->crc) {
        status_msg = "py2g3a runner";
        print_str("This add-in is\ndamaged. Convert it\nagain and re-copy.\n");
        console_draw();
        for (;;) {
            GetKey(&key);
        }
    }

    // Grab as big a Python heap as the OS will give us.
    static const size_t sizes[] = { 160 * 1024, 128 * 1024, 96 * 1024, 64 * 1024, 48 * 1024, 32 * 1024 };
    void *heap = NULL;
    size_t heap_size = 0;
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]) && heap == NULL; i++) {
        heap = malloc(sizes[i]);
        heap_size = sizes[i];
    }
    if (heap == NULL) {
        print_str("Not enough memory.\n");
        console_draw();
        for (;;) {
            GetKey(&key);
        }
    }

    for (;;) {
        console_reset();
        casioplot_reset();
        status_msg = "Running  AC:stop";
        console_draw();
        int failed = run_script(src, len, heap, heap_size, &stack_marker);

        // Finished: let the user read the output, scroll, or run again.
        // A script that drew graphics keeps its picture on screen; any key
        // other than EXE then switches to the text output.
        status_msg = "EXE:again  MENU:quit";
        int show_graphics = casioplot_active && !failed;
        for (;;) {
            if (show_graphics) {
                DefineStatusMessage((char *)status_msg, 1, 0, 0);
                DisplayStatusArea();
                Bdisp_PutDisp_DD();
            } else {
                console_draw();
            }
            GetKey(&key);
            if (show_graphics && key != KEY_CTRL_EXE) {
                show_graphics = 0;
                continue;
            }
            if (key == KEY_CTRL_EXE) {
                break;
            } else if (key == KEY_CTRL_UP) {
                if (count - ROWS - scroll > 0) {
                    scroll++;
                }
            } else if (key == KEY_CTRL_DOWN) {
                if (scroll > 0) {
                    scroll--;
                }
            }
        }
    }
    return 0;
}
