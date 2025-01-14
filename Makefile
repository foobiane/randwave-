directory = $(shell basename "$(CURDIR)")

lib.name = $(directory)
class.sources = src/$(directory).c
datafiles = $(directory)-help.pd

PDLIBBUILDER_DIR=libs/pd-lib-builder
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder