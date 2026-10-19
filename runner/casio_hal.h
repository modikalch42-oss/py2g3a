// Hardware abstraction for the py2g3a runner: stdout goes to the screen,
// input() reads from the keypad. Implemented in main.c.
#ifndef PY2G3A_CASIO_HAL_H
#define PY2G3A_CASIO_HAL_H

#include <stddef.h>

struct _vstr_t;
int casio_readline(struct _vstr_t *line, const char *prompt);
#define mp_hal_readline casio_readline
void mp_hal_set_interrupt_char(int c);

#endif
