
CFLAGS="-Wall -Wextra -ggdb"

cc $CFLAGS -o bangc src/main.c src/lexer.c src/sv.c src/sb.c

