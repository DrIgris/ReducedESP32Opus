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

PCM_OUTPUT_DIR := pcms
OBJECT_DIR := objs
PARSE_FILE := opus_parse
COMPILER := gcc
CCOPTIONS := -Wall

all: file_handling

file_handling: | dirs
> $(COMPILER) $(CCOPTIONS) -c $(PARSE_FILE).c -o $(OBJECT_DIR)/$(PARSE_FILE).o

dirs:
> @mkdir -p $(PCM_OUTPUT_DIR)
> @mkdir -p $(OBJECT_DIR)

clean:
> @rm -rf $(PCM_OUTPUT_DIR)
> @rm -rf $(OBJECT_DIR)

.PHONY: all dirs clean