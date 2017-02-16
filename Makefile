PROJECT = radiance
#CC = gcc
#CC = clang

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	__LINUX__ = true
	CFLAGS = -D__LINUX__
	RADIANCE_LUX = true
endif
ifeq ($(UNAME_S),Darwin)
	__APPLE__ = true
	CFLAGS = -Wno-deprecated-declarations
endif

# Source files
C_SRC  = main.c
C_SRC += $(wildcard audio/*.c)
C_SRC += $(wildcard midi/*.c)
# We'll add back output/lux.c later below if appropriate
C_SRC += $(filter-out output/lux.c, $(wildcard output/*.c))
C_SRC += $(wildcard pattern/*.c)
C_SRC += $(wildcard time/*.c)
C_SRC += $(wildcard ui/*.c)
# We'll add back util/opengl.c later below if appropriate
C_SRC += $(filter-out util/opengl.c, $(wildcard util/*.c))
C_SRC += $(wildcard BTrack/src/*.c)

ifdef __APPLE__
	C_SRC += util/opengl.c
endif

ifdef RADIANCE_LUX
	C_SRC += output/lux.c
	C_SRC += liblux/lux.c liblux/crc.c
endif

OBJDIR = build
$(shell mkdir -p $(OBJDIR) >/dev/null)
OBJECTS = $(C_SRC:%.c=$(OBJDIR)/%.o)

# Compiler flags
INC = -I.

LIBRARIES = -lSDL2 -lSDL2_ttf -lm -lportaudio -lportmidi -lfftw3 -lsamplerate
ifdef __LINUX__
	LIBRARIES += -lGL -lGLU
else
	LIBRARIES += -framework OpenGL
endif

CFLAGS += -std=c99 -ggdb3 -O3 $(INC)
CFLAGS += -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS += -D_POSIX_C_SOURCE=20160524
LFLAGS = $(CFLAGS)

# File dependency generation
DEPDIR = .deps
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPS = $(OBJECTS:$(OBJDIR)/%.o=$(DEPDIR)/%.d)
-include $(DEPS)
$(DEPDIR)/%.d : %.c .deps
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $< -MM -MT $(@:$(DEPDIR)/%.d=%.o) >$@

# Targets
$(PROJECT): $(OBJECTS)
	$(CC) $(LFLAGS) -o $(PROJECT) $(OBJECTS) $(LIBRARIES)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(APP_INC) -c -o $@ $<

# luxctl utility
luxctl: $(OBJDIR)/luxctl.o $(OBJDIR)/liblux/lux.o $(OBJDIR)/liblux/crc.o
	$(CC) $(LFLAGS) -o $@ $^ 

ifdef RADIANCE_LUX
	MAYBE_LUXCTL = luxctl
endif

.PHONY: all
all: $(PROJECT) $(MAYBE_LUXCTL)

.PHONY: clean
clean:
	-rm -f $(PROJECT) tags $(MAYBE_LUXCTL)
	-rm -rf $(OBJDIR) $(DEPDIR)

tags: $(C_SRC)
	ctags --exclude='beat-off/*' -R .

.DEFAULT_GOAL := all
