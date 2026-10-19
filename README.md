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
| **AC/ON** or **EXIT** while it's running | Stops the script |
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

It can't do:

- any graphics (`casioplot`, `turtle`, `matplotlib`). Output is text only, 21 characters by 8 rows, like the built-in Python shell
- importing other `.py` files, so put everything in one script
- `open()` or other file access

Errors show a normal traceback with line numbers.

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
