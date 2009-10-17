DIR=$(shell pwd)
BIN_DIR=$(DIR)/bin
LIB_DIR=$(DIR)/lib
SRC_DIR=$(DIR)/src
INCLUDE_DIR=$(DIR)/include
OBJ_DIR=$(DIR)/obj
DEPS_DIR=$(DIR)/deps
LIBNAME=ccache
LIB_NAME=lib$(LIBNAME).a
LIB=$(LIB_DIR)/lib$(LIBNAME).a
TEST_FIX_CACHE=$(BIN_DIR)/test_fix_cache
TEST_UNFIX_CACHE=$(BIN_DIR)/test_unfix_cache
TESTDIR=$(DIR)/test
INSTALL_DIR=/usr/lib
INSTALL_INCLUDE_DIR=/usr/include/ccache

EXTENSION=c
OBJS=$(patsubst $(SRC_DIR)/%.$(EXTENSION), $(OBJ_DIR)/%.o,$(wildcard $(SRC_DIR)/*.$(EXTENSION)))
DEPS=$(patsubst $(OBJ_DIR)/%.o, $(DEPS_DIR)/%.d, $(OBJS))

INCLUDE_FILES = ccache.h ccache_error.h ccache_hash.h ccache_node.h
INCLUDE=-I$(INCLUDE_DIR)

CC=gcc
STRIP=strip
CONFIGURE=-DCCACHE_USE_RBTREE
CFLAGS=-Wall -W -g 
STRIP_FLAGS=-g

.PHONY: all clean install uninstall rebuild

all:$(OBJS)
	ar rcs $(LIB) $(OBJS)

sinclude $(DEPS)

$(DEPS_DIR)/%.d: $(SRC_DIR)/%.$(EXTENSION)
	@$(CC) -MM $(INCLUDE) $< > temp; \
	sed 1's,^,$(OBJ_DIR)/,' < temp > $@; \
	rm -f temp

$(OBJ_DIR)/%.o:$(SRC_DIR)/%.$(EXTENSION) 
	$(CC) $< -o $@ -c $(CFLAGS) $(CONFIGURE) $(INCLUDE) 

test_fix_cache:test/test_fix_cache.c $(LIB)
	$(CC) -o $(TEST_FIX_CACHE) $(TESTDIR)/test_fix_cache.c -L$(INSTALL_DIR) -l$(LIBNAME) $(CFLAGS) -lpthread

test_unfix_cache:test/test_unfix_cache.c $(LIB)
	$(CC) -o $(TEST_UNFIX_CACHE) $(TESTDIR)/test_unfix_cache.c -L$(INSTALL_DIR) -l$(LIBNAME) $(CFLAGS) -lpthread

install:
	$(STRIP) $(STRIP_FLAGS) $(LIB)
	cp $(LIB) $(INSTALL_DIR)
	test -d $(INSTALL_INCLUDE_DIR) || mkdir $(INSTALL_INCLUDE_DIR)
	for i in $(INCLUDE_FILES); do 			\
		cd $(INCLUDE_DIR); 				\
		cp $$i $(INSTALL_INCLUDE_DIR); 	\
	done

uninstall:
	rm -f $(INSTALL_DIR)/$(LIB_NAME) 
	rm -fr ${INSTALL_INCLUDE_DIR}

rebuild: clean all

clean:
	rm -rf $(OBJ_DIR)/* $(LIB_DIR)/* $(BIN_DIR)/*
	rm -fr $(DEPS_DIR)/*

