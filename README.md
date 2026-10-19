# py2g3a

Turns a Python script into a Casio **fx-CG50 / fx-CG20 / fx-CG10** add-in (`.g3a`), so each script gets **its own icon in MAIN MENU**.

Works on Windows, macOS and Linux. All you need is Python 3.8 or newer. There's nothing to install.

## Quick start

1. Keep `py2g3a.py` and `runner.g3a` in the same folder.
2. Convert a script:

   ```
   python py2g3a.py snake.py
   ```

   This makes `snake.g3a`. On macOS or Linux you may need `python3` instead of `python`.

   To pick the menu name or icon:

   ```
   python py2g3a.py snake.py -n "Snake" -i snake.png
   ```

   You can also convert several files at once: `python py2g3a.py *.py`

   If you'd rather not use a terminal, run `python py2g3a.py` with no arguments (or double-click it on Windows). It opens a file picker instead.
3. Plug the calculator in and choose **USB Flash** on its screen. Copy the `.g3a` to the top level of the calculator's drive, then eject it properly.
4. The app is now in MAIN MENU. Open it and the script runs.

## On the calculator

| Key | What it does |
|---|---|
| **AC/ON** while it's running | Stops the script (EXIT also works, unless the script uses `casioplot`) |
| **EXE** after it finishes | Runs it again |
| **▲ / ▼** after it finishes | Scrolls back through the output |
| **MENU** | Leaves the app |
| **DEL** in `input()` | Backspace |

When you type into `input()`, use ALPHA for letters (SHIFT then ALPHA locks it). The ^ key types `**` and ×10ˣ types `e`, so `2e5` works.

## What Python you get

The runner is MicroPython 1.24, which has the same core language as the calculator's built-in Python but is newer. You get:

- `print`, `input`, f-strings, lists, dicts, sets, classes, generators, exceptions and big integers (`2**1000` works)
- double-precision floats
- `math` (including `factorial`, `gamma`, `isclose`) and `random`
- `casioplot` for graphics and games (see below)
- `time`: `sleep()`, `sleep_ms()`, `ticks_ms()`, `monotonic()`

Text output from `print` is 21 characters by 8 rows, like the built-in Python shell.

It can't do:

- `turtle` or `matplotlib`
- importing other `.py` files, so put everything in one script
- `open()` or other file access

Errors show a normal traceback with line numbers.

## Graphics and games: casioplot

`casioplot` works like Casio's own module: a 384×192 drawing area under the status bar, where nothing appears until you call `show_screen()`.

| Function | What it does |
|---|---|
| `clear_screen()` | Clears the drawing area to white |
| `show_screen()` | Puts what you've drawn on the display |
| `set_pixel(x, y, colour)` | Sets one pixel. `colour` is `(r, g, b)` with 0–255 values, black by default |
| `get_pixel(x, y)` | Returns the pixel's `(r, g, b)`, or `None` if it's off-screen |
| `draw_string(x, y, text, colour, size)` | Draws text with its top-left corner at `(x, y)`. `size` is `"small"`, `"medium"` (the default) or `"large"` |

Two extras aren't in Casio's fx-CG50 version but make games possible:

| Function | What it does |
|---|---|
| `fill_rect(x, y, w, h, colour)` | Draws a filled rectangle. Much faster than a loop of `set_pixel` calls |
| `getkey()` | Returns the key being held down right now, or `0`, without waiting |

`getkey()` returns the same codes as Casio BASIC's `Getkey`, and `casioplot` has names for the common ones: `KEY_LEFT`, `KEY_RIGHT`, `KEY_UP`, `KEY_DOWN`, `KEY_EXE`, `KEY_EXIT`, `KEY_SHIFT`, `KEY_ALPHA`, `KEY_OPTN`, `KEY_F1` to `KEY_F6` and `KEY_0` to `KEY_9`. It sees one key at a time.

A game loop looks like this:

```python
from casioplot import clear_screen, show_screen, fill_rect, getkey, KEY_LEFT, KEY_RIGHT
import time

x = 180
while True:
    k = getkey()
    if k == KEY_LEFT:
        x -= 4
    elif k == KEY_RIGHT:
        x += 4
    fill_rect(0, 0, 384, 192, (255, 255, 255))
    fill_rect(x, 170, 30, 8, (0, 0, 200))
    show_screen()
    time.sleep_ms(30)
```

`examples/breakout.py` is a complete game. Convert it with `python py2g3a.py examples/breakout.py`, or copy the ready-made `examples/breakout.g3a` straight to the calculator.

When a script that draws finishes, its picture stays on screen. Press EXE to run it again, or any other key to see its text output. If the script stops with an error, you see the error straight away. Calling `input()` switches to the text screen, and the next drawing call clears the drawing area.

The clock behind `time` ticks 128 times a second, so timings are accurate to about 8 ms.

## Icons

If you don't give an icon, py2g3a draws one: a coloured tile with the app's name under it. Each app gets a colour based on its name.

`-i` takes any PNG and fits it into 92×64 on a white background. Casio's own icons have the name inside the picture, so you may want to add text to yours. `-s` sets a separate image for when the icon is highlighted.

## How it works

A `.g3a` has to contain compiled SuperH machine code, so a `.py` file can't be converted into one directly. `runner.g3a` is a small MicroPython interpreter compiled for the calculator. It has an empty "slot" at the very end of the file.

`py2g3a.py` copies the runner, puts your script in the slot, and gives the copy its own name, internal ID and icons. It then fixes the size fields and checksums so the calculator treats the copy as a new add-in. When the add-in starts, it checks the script against a CRC and runs it.

Each `.g3a` is about 200 KB, because each one carries its own copy of the interpreter.

## Rebuilding runner.g3a (optional)

You don't need to do this. A prebuilt `runner.g3a` is included. If you want to change the runner, the source is in `runner/`, and `runner/build.sh` fetches and builds everything on Linux or WSL: the SH cross compiler, libfxcg, mkg3a and MicroPython.

## Troubleshooting

- **"can't find runner.g3a"**: keep `runner.g3a` next to `py2g3a.py`, or pass `-r path/to/runner.g3a`.
- **The app doesn't show in MAIN MENU**: make sure the file is at the top level of the calculator's drive, not in a folder, and that you ejected the drive before unplugging.
- **"This add-in is damaged"**: the file got cut short while copying. Copy it again.
- **Exam Mode** turns off all add-ins. That's the calculator, not this tool.
