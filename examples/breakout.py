# Breakout for the fx-CG50, using casioplot.
#   python py2g3a.py examples/breakout.py
# Controls: LEFT / RIGHT move, EXE launches the ball, AC/ON quits.
from casioplot import clear_screen, show_screen, fill_rect, draw_string, getkey
from casioplot import KEY_LEFT, KEY_RIGHT, KEY_EXE
import time

W, H = 384, 192
COLS, ROWS = 8, 5
BW, BH, GAP, TOP = 44, 10, 3, 26
ROW_COLOURS = [(220, 60, 60), (235, 140, 40), (230, 200, 40), (60, 170, 80), (50, 120, 220)]
PW, PH, PY = 60, 6, 178
BALL = 5
BG = (15, 20, 40)

def new_bricks():
    return [[True] * COLS for _ in range(ROWS)]

def brick_x(c):
    return GAP + c * (BW + GAP)

def brick_y(r):
    return TOP + r * (BH + GAP)

def draw(bricks, px, bx, by, score, lives, msg):
    fill_rect(0, 0, W, H, BG)
    for r in range(ROWS):
        row = bricks[r]
        colour = ROW_COLOURS[r]
        for c in range(COLS):
            if row[c]:
                fill_rect(brick_x(c), brick_y(r), BW, BH, colour)
    fill_rect(int(px), PY, PW, PH, (230, 230, 240))
    fill_rect(int(bx), int(by), BALL, BALL, (255, 255, 255))
    draw_string(4, 4, "Score " + str(score), (255, 255, 255), "small")
    draw_string(W - 52, 4, "Lives " + str(lives), (255, 255, 255), "small")
    if msg:
        draw_string(W // 2 - 6 * len(msg) // 2 - 20, 120, msg, (255, 220, 120), "medium")
    show_screen()

def play():
    bricks = new_bricks()
    left = ROWS * COLS
    score, lives = 0, 3
    px = (W - PW) / 2
    stuck = True
    bx, by, vx, vy = 0.0, 0.0, 0.0, 0.0
    msg = "EXE to launch"
    while True:
        start = time.ticks_ms()
        k = getkey()
        if k == KEY_LEFT:
            px = max(0, px - 7)
        elif k == KEY_RIGHT:
            px = min(W - PW, px + 7)

        if stuck:
            bx, by = px + PW / 2 - BALL / 2, PY - BALL - 1
            if k == KEY_EXE:
                if msg.startswith("You") or msg.startswith("Game"):
                    return True
                stuck, msg = False, ""
                vx, vy = 2.5, -3.5
        else:
            bx += vx
            by += vy
            # Walls.
            if bx < 0:
                bx, vx = 0, -vx
            elif bx > W - BALL:
                bx, vx = W - BALL, -vx
            if by < 0:
                by, vy = 0, -vy
            # Paddle: the further from the centre it hits, the sharper the angle.
            if vy > 0 and PY - BALL <= by <= PY + PH and px - BALL < bx < px + PW:
                by = PY - BALL
                offset = (bx + BALL / 2 - (px + PW / 2)) / (PW / 2)
                offset = max(-0.85, min(0.85, offset))
                vx = 4.5 * offset
                vy = -(4.5 * 4.5 - vx * vx) ** 0.5
            # Bricks: look at the cell under the ball's centre.
            cx, cy = bx + BALL / 2, by + BALL / 2
            r = int((cy - TOP) // (BH + GAP))
            c = int((cx - GAP) // (BW + GAP))
            if 0 <= r < ROWS and 0 <= c < COLS and bricks[r][c]:
                x0, y0 = brick_x(c), brick_y(r)
                if x0 <= cx <= x0 + BW and y0 <= cy <= y0 + BH:
                    bricks[r][c] = False
                    left -= 1
                    score += (ROWS - r) * 10
                    # Bounce off whichever side the ball went in through.
                    over_x = min(cx - x0, x0 + BW - cx)
                    over_y = min(cy - y0, y0 + BH - cy)
                    if over_x < over_y:
                        vx = -vx
                    else:
                        vy = -vy
                    if left == 0:
                        stuck, msg = True, "You win! EXE"
            # Missed.
            if by > H:
                lives -= 1
                stuck = True
                msg = "EXE to launch" if lives > 0 else "Game over  EXE"
                if lives == 0:
                    lives = 0

        draw(bricks, px, bx, by, score, lives, msg)
        # Aim for about 30 frames a second.
        spare = 33 - (time.ticks_ms() - start)
        if spare > 0:
            time.sleep_ms(spare)

clear_screen()
while play():
    pass
