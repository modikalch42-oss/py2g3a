# Solves ax^2 + bx + c = 0
import math

print("ax^2 + bx + c = 0")
a = float(input("a = "))
b = float(input("b = "))
c = float(input("c = "))
d = b * b - 4 * a * c
if a == 0:
    print("Not a quadratic")
elif d < 0:
    re = -b / (2 * a)
    im = math.sqrt(-d) / (2 * a)
    print("x =", round(re, 6), "+/-", round(abs(im), 6), "i")
elif d == 0:
    print("x =", -b / (2 * a))
else:
    print("x1 =", (-b + math.sqrt(d)) / (2 * a))
    print("x2 =", (-b - math.sqrt(d)) / (2 * a))
