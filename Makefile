#!/usr/bin/make -f

CC=gcc
LIBS=purple json-c

PKG_CONFIG=pkg-config

PKG_CFLAGS:=$(shell $(PKG_CONFIG) --cflags $(LIBS) || echo "FAILED")
ifeq ($(PKG_CFLAGS),FAILED)
	$(error "$(PKG_CONFIG) failed")
endif
CFLAGS+=$(PKG_CFLAGS) -fPIC -DPIC

PKG_LDLIBS:=$(shell $(PKG_CONFIG) --libs $(LIBS) || echo "FAILED")
ifeq ($(PKG_LDLIBS),FAILED)
	$(error "$(PKG_CONFIG) failed")
endif
LDLIBS+=$(PKG_LDLIBS)

PLUGIN_DIR_PURPLE	=  $(shell $(PKG_CONFIG) --variable=plugindir purple)
DATA_ROOT_DIR_PURPLE	=  $(shell $(PKG_CONFIG) --variable=datarootdir purple)

TARGET=libpurple-oicq.so

include Makefile.common
