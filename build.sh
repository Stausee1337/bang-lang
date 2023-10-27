
CFLAGS="-Wall -Wextra -ggdb"

clang $CFLAGS -o bangc src/main.c src/lexer.c src/utf8.c src/dynarray.c

