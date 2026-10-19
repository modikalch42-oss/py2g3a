# Number guessing game - try converting this one first:
#   python py2g3a.py examples/guess.py -n "Guess"
import random

secret = random.randint(1, 100)
print("I'm thinking of a")
print("number from 1 to 100.")
tries = 0
while True:
    guess = input("Guess: ")
    if not guess.isdigit():
        print("Numbers only!")
        continue
    tries += 1
    guess = int(guess)
    if guess < secret:
        print("Higher")
    elif guess > secret:
        print("Lower")
    else:
        print("Got it in", tries, "tries!")
        break
