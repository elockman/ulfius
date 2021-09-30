#
# WebUI example program
#
# Makefile used to build the software
#
# Copyright 2015-2017 Nicolas Mora <mail@babelouest.org>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the MIT License
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
CC=gcc
ULFIUS_LOCATION=../../src
ULFIUS_INCLUDE=../../include
EXAMPLE_INCLUDE=../include
STATIC_COMPRESSED_INMEMORY_WEBSITE=../../example_callbacks/static_compressed_inmemory_website
HTTP_COMRESSION=../../example_callbacks/http_compression
CFLAGS+=-c -Wall -I$(ULFIUS_INCLUDE) -I$(EXAMPLE_INCLUDE) -I$(STATIC_COMPRESSED_INMEMORY_WEBSITE) -I$(HTTP_COMRESSION) -D_REENTRANT $(ADDITIONALFLAGS) $(CPPFLAGS)
LIBS=-lc -lz -lpthread -lulfius -lorcania -L$(ULFIUS_LOCATION)

ifndef YDERFLAG
LIBS+= -lyder
endif

all: webui_app

clean:
	rm -f *.o webui_app
	rm -f *.o cJSON

debug: ADDITIONALFLAGS=-DDEBUG -g -O0

debug: webui_app

../../src/libulfius.so:
	cd $(ULFIUS_LOCATION) && $(MAKE) debug CURLFLAG=1 GNUTLSFLAG=1

static_compressed_inmemory_website_callback.o: $(STATIC_COMPRESSED_INMEMORY_WEBSITE)/static_compressed_inmemory_website_callback.c
	$(CC) $(CFLAGS) $(STATIC_COMPRESSED_INMEMORY_WEBSITE)/static_compressed_inmemory_website_callback.c

http_compression_callback.o: $(HTTP_COMRESSION)/http_compression_callback.c
	$(CC) $(CFLAGS) $(HTTP_COMRESSION)/http_compression_callback.c

cJSON.o: cJSON.c
	$(CC) $(CFLAGS) cJSON.c

webui_app.o: webui_app.c cJSON.c
	$(CC) $(CFLAGS) webui_app.c cJSON.c -DDEBUG -g -O0

webui_app: ../../src/libulfius.so webui_app.o cJSON.o static_compressed_inmemory_website_callback.o http_compression_callback.o
	$(CC) -o webui_app webui_app.o cJSON.o static_compressed_inmemory_website_callback.o http_compression_callback.o $(LIBS)

test: webui_app
	LD_LIBRARY_PATH=$(ULFIUS_LOCATION):${LD_LIBRARY_PATH} ./webui_app
