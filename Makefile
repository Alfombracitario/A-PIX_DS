# -------------------------------------------------
# Ruta a BlocksDS
# -------------------------------------------------
BLOCKSDS ?= /opt/blocksds/core

# -------------------------------------------------
# Librerías
# -------------------------------------------------
LIBS        := -lnds9 -lmm9
LIBDIRS     := $(BLOCKSDS)/libs/maxmod

# -------------------------------------------------
# Información del ROM
# -------------------------------------------------
NAME            := A-Pix
GAME_TITLE      := A-Pix DS
GAME_SUBTITLE   := @Alfombracitario
GAME_AUTHOR     := 0.5.1
GAME_ICON 		:= icon.gif

# -------------------------------------------------
# Carpetas del proyecto
# -------------------------------------------------
SOURCEDIRS      := source
INCLUDEDIRS     := include
GFXDIRS         := graphics
AUDIODIRS       := audio
# -------------------------------------------------
# Makefile base BlocksDS
# -------------------------------------------------
include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile
