CFLAGS = `pkg-config --cflags libcurl libcjson` -Iid3v2lib/include
LFLAGS = `pkg-config --libs   libcurl libcjson` -Lid3v2lib/src -lid3v2

all:
	cd ./id3v2lib && cmake .
	$(MAKE) -C ./id3v2lib
	clang -Wall -Werror --pedantic-errors --std=c11 $(CFLAGS) main.c urlencode.c $(LFLAGS)
