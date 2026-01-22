.PHONY: all

all: 
	tsc -w joc.ts & \
	live-server .