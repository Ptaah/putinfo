CC	= gcc

NAME    = info
LIBNAME	= lib$(NAME)
BINNAME = put$(NAME)

LIBSRC	= $(LIBNAME).c
BINSRC  = putinfo.c
INC     = info.h

OBJ	= $(LIBSRC:.c=.o)
CFLAGS	= -O3 -fPIC -Wall -Werror -Wextra
LDFLAGS = -L. -l $(NAME) -lsqlite3 -lbsd

all	: $(BINNAME)

$(OBJ)  : $(LIBSRC) $(INC)
	$(CC) $(CFLAGS)	$< -c

$(LIBNAME).a	: $(OBJ)
	ar -r $(LIBNAME).a $(OBJ)
	ranlib $(LIBNAME).a

$(LIBNAME).so	: $(OBJ)
	$(CC) $< -o $(LIBNAME).so -shared $(CFLAGS)

$(BINNAME) : $(LIBNAME).so
	$(CC) $(CFLAGS) $(BINSRC) -o $(BINNAME) $(LDFLAGS)

clean	:
	rm -f $(OBJ) $(BINNAME) $(LIBNAME).{a,so} *~ core *.core
