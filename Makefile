.PHONY: all

all: 
	tsc -w joc/joc.ts & \
	live-server .