CC	= gcc

NAME    = info
LIBNAME	= lib$(NAME)
BINNAME = put$(NAME)

LIBSRC	= $(LIBNAME).c
BINSRC  = putinfo.c
INC     = info.h

OBJ	= $(LIBSRC:.c=.o)
CFLAGS	= -O3 -fPIC -Wall -Werror -Wextra -std=gnu99 -g
LDFLAGS = -L. -l $(NAME) $(shell pkgconf --libs sqlite3)

all	: $(BINNAME) $(BINNAME)_static

$(OBJ)  : $(LIBSRC) $(INC) Makefile
	$(CC) $(CFLAGS)	$< -c

$(LIBNAME).a	: $(OBJ)
	ar -r $(LIBNAME).a $(OBJ)
	ranlib $(LIBNAME).a

$(LIBNAME).so	: $(OBJ)
	$(CC) $< -o $(LIBNAME).so -shared $(CFLAGS)

$(BINNAME) : $(LIBNAME).so $(BINSRC)
	$(CC) $(CFLAGS) $(BINSRC) -o $@ $(LDFLAGS)

$(BINNAME)_static : $(LIBNAME).a $(BINSRC)
	$(CC) $(CFLAGS) $(BINSRC) $(LIBNAME).a -o $@  $(shell pkgconf --libs --static sqlite3)

clean	:
	rm -f $(OBJ) $(BINNAME) $(BINNAME)_static $(LIBNAME).{a,so} *~ core *.core
