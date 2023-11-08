
CFLAGS="-Wall -Wextra -ggdb"

cc $CFLAGS -o out/bangc src/main.c src/lexer.c src/strings.c

