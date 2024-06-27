.PHONY: all clean

all: libmsocket.a initmsocket users

libmsocket.a:
	make -f Makefile_libmsocket

initmsocket:
	make -f Makefile_initmsocket

users:
	make -f Makefile_users

clean:
	make -f Makefile_libmsocket clean
	make -f Makefile_initmsocket clean
	make -f Makefile_users clean