CC=gcc
CFLAGS=-Wall -Wextra -ggdb
PYTHONPATH=/home/stausee1337/MISC/bang_lang/Tools:/home/stausee1337/MISC/bang_lang/Generators

all: thirdparty templ8 out/bangc
templ8: src/*.generated.h

thirdparty: Thirdparty/csiphash.o

out/bangc: src/*.c src/*.h Thirdparty/*.o
	$(CC) $(CFLAGS) -o out/bangc src/lexer.c src/main.c src/strings.c Thirdparty/csiphash.o

src/%.generated.h: src/%.h.templ8
	PYTHONPATH=$(PYTHONPATH) python3 -m Templ8 $<

Thirdparty/csiphash.o: Thirdparty/csiphash.c
	$(CC) -o Thirdparty/csiphash.o -c Thirdparty/csiphash.c

