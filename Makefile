
BLOCKSDS	?= /opt/blocksds/core

LIBS        := -lnds9 -lmm9
LIBDIRS     := $(BLOCKSDS)/libs/maxmod

NAME            := A-Pix
GAME_TITLE      := A-Pix DS
GAME_SUBTITLE   := Beta 0.7 for jShop
GAME_AUTHOR     := Alfombracitario
GAME_ICON 		:= icon.gif

SOURCEDIRS      := source
INCLUDEDIRS     := include
GFXDIRS         := graphics
AUDIODIRS       := audio

NITROFSDIR	:= nitrofs

include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile