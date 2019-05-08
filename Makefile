TARGETS=ringmaster player

all: $(TARGETS)
clean:
	rm -f $(TARGETS) *~

ringmaster: ringmaster.c
	gcc -g -Wall -Werror -pedantic -std=gnu99 -o $@ $<

player: player.c
	gcc -g -Wall -Werror -pedantic -std=gnu99 -o $@ $<
