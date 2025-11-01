all: spell

spell: spell.o
	gcc -Wall -Wextra -g spell.o -o spell

%.o: %.c
	gcc -Wall -Wextra -g -c $< -o $@

clean:
	rm -f spell.o spell 