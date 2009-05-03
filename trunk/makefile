######################################################################
#	makefile 模版 (by lichuang)
#
######################################################################

###################项目路径和程序名称#################################
DIR=$(shell pwd)
BIN_DIR=$(DIR)/bin
LIB_DIR=$(DIR)/lib
SRC_DIR=$(DIR)/src
INCLUDE_DIR=$(DIR)/include
OBJ_DIR=$(DIR)/obj
DEPS_DIR=$(DIR)/deps
LIBNAME=ccache
LIB=$(LIB_DIR)/lib$(LIBNAME).a
TEST_FIX_CACHE=$(BIN_DIR)/test_fix_cache
TEST_UNFIX_CACHE=$(BIN_DIR)/test_unfix_cache
TESTDIR=$(DIR)/test

###################OBJ文件及路径############################################
EXTENSION=c
OBJS=$(patsubst $(SRC_DIR)/%.$(EXTENSION), $(OBJ_DIR)/%.o,$(wildcard $(SRC_DIR)/*.$(EXTENSION)))
DEPS=$(patsubst $(OBJ_DIR)/%.o, $(DEPS_DIR)/%.d, $(OBJS))

###################include头文件路径##################################
INCLUDE=\
		-I$(INCLUDE_DIR)
		
###################lib文件及路径######################################

###################编译选项及编译器###################################
CC=gcc
#CFLAGS=-Wall -W -g -DCCACHE_USE_LIST
CFLAGS=-Wall -W -g -DCCACHE_USE_RBTREE

###################编译目标###########################################
.PHONY: all clean rebuild

all:$(OBJS)
	ar rcs $(LIB) $(OBJS)

sinclude $(DEPS)

$(DEPS_DIR)/%.d: $(SRC_DIR)/%.$(EXTENSION)
	@$(CC) -MM $(INCLUDE) $< > temp; \
	sed 1's,^,$(OBJ_DIR)/,' < temp > $@; \
	rm -f temp

$(OBJ_DIR)/%.o:$(SRC_DIR)/%.$(EXTENSION) 
	$(CC) $< -o $@ -c $(CFLAGS) $(INCLUDE) 

test_fix_cache:test/test_fix_cache.c $(LIB)
	$(CC) -o $(TEST_FIX_CACHE) $(TESTDIR)/test_fix_cache.c -L$(LIB_DIR) -l$(LIBNAME) $(CFLAGS) $(INCLUDE) -lpthread

test_unfix_cache:test/test_unfix_cache.c $(LIB)
	$(CC) -o $(TEST_UNFIX_CACHE) $(TESTDIR)/test_unfix_cache.c -L$(LIB_DIR) -l$(LIBNAME) $(CFLAGS) $(INCLUDE) -lpthread

rebuild: clean all

clean:
	rm -rf $(OBJ_DIR)/* $(LIB_DIR)/* $(BIN_DIR)/*

