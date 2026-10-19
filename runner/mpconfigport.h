// MicroPython configuration for the py2g3a runner (Casio fx-CG10/20/50, SH4A, big-endian).
#include <stdint.h>

// Start from the core feature set, then add what a calculator user expects.
#define MICROPY_CONFIG_ROM_LEVEL            (MICROPY_CONFIG_ROM_LEVEL_CORE_FEATURES)

#define MICROPY_ENABLE_COMPILER             (1)
#define MICROPY_ENABLE_GC                   (1)
#define MICROPY_PY_GC                       (1)
#define MICROPY_HELPER_REPL                 (0)
#define MICROPY_ENABLE_EXTERNAL_IMPORT      (0)   // only built-in modules (no files)

// No SuperH-specific assembly exists, so use setjmp for exceptions and the GC.
#define MICROPY_NLR_SETJMP                  (1)
#define MICROPY_GCREGS_SETJMP               (1)

// Numbers: double-precision floats and arbitrary-size integers.
#define MICROPY_FLOAT_IMPL                  (MICROPY_FLOAT_IMPL_DOUBLE)
#define MICROPY_LONGINT_IMPL                (MICROPY_LONGINT_IMPL_MPZ)
#define MICROPY_PY_MATH                     (1)
#define MICROPY_PY_MATH_SPECIAL_FUNCTIONS   (1)
#define MICROPY_PY_MATH_FACTORIAL           (1)
#define MICROPY_PY_MATH_ISCLOSE             (1)
#define MICROPY_PY_CMATH                    (0)

// Handy built-ins and modules.
#define MICROPY_PY_BUILTINS_INPUT           (1)
#define MICROPY_PY_BUILTINS_STR_UNICODE     (1)
#define MICROPY_PY_BUILTINS_ROUND_INT       (1)
#define MICROPY_PY_ALL_SPECIAL_METHODS      (1)
#define MICROPY_PY_FSTRINGS                 (1)
#define MICROPY_PY_BUILTINS_STR_CENTER      (1)
#define MICROPY_PY_BUILTINS_STR_PARTITION   (1)
#define MICROPY_PY_BUILTINS_STR_SPLITLINES  (1)
#define MICROPY_PY_BUILTINS_SLICE_INDICES   (1)
#define MICROPY_PY_BUILTINS_FROZENSET       (1)
#define MICROPY_PY_BUILTINS_NOTIMPLEMENTED  (1)
#define MICROPY_PY_REVERSE_SPECIAL_METHODS  (1)
#define MICROPY_PY_DESCRIPTORS              (1)
#define MICROPY_PY_MATH_CONSTANTS           (1)
#define MICROPY_CPYTHON_COMPAT              (1)
#define MICROPY_PY_RANDOM                   (1)
#define MICROPY_PY_RANDOM_EXTRA_FUNCS       (1)
#define MICROPY_PY_RANDOM_SEED_INIT_FUNC    (casio_random_seed())
#define MICROPY_PY_IO                       (0)
#define MICROPY_PY_SYS_STDFILES             (0)

// Errors: show line numbers in tracebacks.
#define MICROPY_ENABLE_SOURCE_LINE          (1)
#define MICROPY_ERROR_REPORTING             (MICROPY_ERROR_REPORTING_NORMAL)

// AC/ON or EXIT stops a running script (raises KeyboardInterrupt).
#define MICROPY_KBD_EXCEPTION               (1)
#define MICROPY_VM_HOOK_COUNT               (2000)
#define MICROPY_VM_HOOK_INIT                static unsigned int vm_hook_divisor = MICROPY_VM_HOOK_COUNT;
#define MICROPY_VM_HOOK_POLL                if (--vm_hook_divisor == 0) { vm_hook_divisor = MICROPY_VM_HOOK_COUNT; casio_poll(); }
#define MICROPY_VM_HOOK_LOOP                MICROPY_VM_HOOK_POLL
#define MICROPY_VM_HOOK_RETURN              MICROPY_VM_HOOK_POLL

// Deep recursion raises RuntimeError instead of crashing the calculator.
#define MICROPY_STACK_CHECK                 (1)

#define MICROPY_ALLOC_PATH_MAX              (128)
#define MICROPY_HW_BOARD_NAME               "Casio fx-CG"
#define MICROPY_HW_MCU_NAME                 "SH4A"
#define MICROPY_PY_SYS_PLATFORM             "casio"

// Type definitions for the machine (32-bit).
typedef intptr_t mp_int_t;
typedef uintptr_t mp_uint_t;
typedef long mp_off_t;

#include <alloca.h>

#ifdef TARGET_PRIZM
#include <stddef.h>
typedef int ssize_t;   // libfxcg's libc doesn't define it
#endif

#define MICROPY_MPHALPORT_H                 "casio_hal.h"

// Declared here so the hooks above can call them from anywhere.
void casio_poll(void);
unsigned int casio_random_seed(void);
