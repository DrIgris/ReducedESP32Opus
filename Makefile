SHELL := bash
.ONESHELL:
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

ifeq ($(origin .RECIPEPREFIX), undefined)
  $(error This Make does not support .RECIPEPREFIX. Please use GNU Make 4.0 or later)
endif
.RECIPEPREFIX = >

COMPILER := gcc
CCOPTIONS := -Wall -MMD -MP -std=c99

PCM_OUTPUT_DIR := pcms
OBJECT_DIR := objs

OBJECTS := $(OBJECT_DIR)/celt.o \
           $(OBJECT_DIR)/modes.o \
           $(OBJECT_DIR)/decoder.o \
           $(OBJECT_DIR)/opus_parse.o \
           $(OBJECT_DIR)/opus_custom.o \
           $(OBJECT_DIR)/main.o

all: $(OBJECT_DIR)/main

$(OBJECT_DIR)/main: $(OBJECTS) | $(OBJECT_DIR)
> $(COMPILER) $^ -o $@

$(OBJECT_DIR)/%.o: %.c | $(OBJECT_DIR)
> $(COMPILER) $(CCOPTIONS) -c $< -o $@

$(OBJECT_DIR):
> mkdir -p $(OBJECT_DIR)

$(PCM_OUTPUT_DIR):
> mkdir -p $(PCM_OUTPUT_DIR)

-include $(wildcard $(OBJECT_DIR)/*.d)

clean:
> @rm -rf $(PCM_OUTPUT_DIR)
> @rm -rf $(OBJECT_DIR)

.PHONY: all clean