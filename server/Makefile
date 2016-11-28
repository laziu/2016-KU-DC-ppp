# $^ : dependency list
# $@ : target
INC_DIR=include
OBJ_DIR=obj
SRC_DIR=src

CC=gcc
CFLAGS=-lrt -I $(INC_DIR) -std=gnu99

TARGET=pppimpl
_OBJS=main.o utils.o physical.o ahdlc.o ppp.o lcp.o ipcp.o
_HEADERS=common.h utils.h physical.h ahdlc.h ppp.h lcp.h ipcp.h
OBJS=$(patsubst %,$(OBJ_DIR)/%,$(_OBJS))
HEADERS=$(patsubst %,$(INC_DIR)/%,$(_HEADERS))

all: debug

debug: CFLAGS+=-g -O0 -D DEBUG
debug: $(TARGET)

release: CFLAGS+=-O3
release: $(TARGET)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) 

.PHONY: all debug release clean

clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET)
