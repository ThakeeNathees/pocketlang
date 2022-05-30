
##  Copyright (c) 2020-2021 Thakee Nathees
##  Distributed Under The MIT License

CC             = gcc
CFLAGS         = -fPIC
DEBUG_CFLAGS   = -D DEBUG -g3 -Og
RELEASE_CFLAGS = -g -O3
LDFLAGS        = -lm -ldl

TARGET_EXEC = pocket
BUILD_DIR   = ./build/

SRC_DIRS = ./cli/ ./src/core/ ./src/libs/
INC_DIRS = ./src/include/

BIN_DIR = bin/
OBJ_DIR = obj/

SRCS := $(foreach DIR,$(SRC_DIRS),$(wildcard $(DIR)*.c))
OBJS := $(SRCS:.c=.o)
DEPS := $(OBJS:.o=.d)

INC_FLAGS := $(addprefix -I,$(INC_DIRS))
DEP_FLAGS  = -MMD -MP
CC_FLAGS   = $(INC_FLAGS) $(DEP_FLAGS) $(CFLAGS)

DEBUG_DIR     = $(BUILD_DIR)Debug/
DEBUG_TARGET  = $(DEBUG_DIR)$(BIN_DIR)$(TARGET_EXEC)
DEBUG_OBJS   := $(addprefix $(DEBUG_DIR)$(OBJ_DIR), $(OBJS))

RELEASE_DIR     = $(BUILD_DIR)Release/
RELEASE_TARGET  = $(RELEASE_DIR)$(BIN_DIR)$(TARGET_EXEC)
RELEASE_OBJS   := $(addprefix $(RELEASE_DIR)$(OBJ_DIR), $(OBJS))

.PHONY: debug release all clean

# default; target if run as `make`
debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(DEBUG_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

$(DEBUG_DIR)$(OBJ_DIR)%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CC_FLAGS) $(DEBUG_CFLAGS) -c $< -o $@

release: $(RELEASE_TARGET)

$(RELEASE_TARGET): $(RELEASE_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

$(RELEASE_DIR)$(OBJ_DIR)%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CC_FLAGS) $(RELEASE_CFLAGS) -c $< -o $@

all: debug release

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
