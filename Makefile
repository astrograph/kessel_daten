# Choose Toolchain
# ##################

# LINUX nativ compiler
# --------------
TOOL = /usr/bin/

# CYGWIN WIN32 Compiler
# --------------
# TOOL = /bin/

# MinGW WIN32 Compiler
# --------------
#TOOL = C:/MinGW/bin/

# CYGWIN x686 Linux Compiler
# --------------
# TOOL= /opt/crosstool/fc4/gcc-4.1.0-glibc-2.3.6/i686-unknown-linux-gnu/bin/i686-unknown-linux-gnu-

# CYGWIN cuLibc Linux compiler for ARM (buildroot project)
# --------------
#TOOL = /opt/buildroot/build_arm/staging_dir/bin/arm-linux-

# CYGWIN cuLibc linux compiler for i386 (buildroot project)
# --------------
#TOOL = /opt/buildroot/build_i486/staging_dir/bin/i486-linux-

# BUILROOT cuLibc linux compiler for i386 (buildroot project)
# --------------
# TOOL = /home/buildroot/build_i386/staging_dir/usr/bin/i386-linux-

# Basic C source files 
# ---------------------------------------------------------------------------
C_SRCS =  froeling_p3100_logger.c \
# print_log.c

# Target name
# ##################
TARGET = froeling_p3100_logger

# Choose Compiler Settings
# ##################

# DEBUG VERSION
# --------------
#CFLAGS  = -g -Wall -fmessage-length=0

# RELEASE VERSION
# --------------
CFLAGS  = -O -Wall -fmessage-length=0

CC = $(TOOL)gcc
LINK = $(CC)
STRIP = $(TOOL)strip

# Do not edit
# ##################

# Objects
OBJS =	$(C_SRCS:.c=.o)

# Dependencies
C_DEPS += 

# Libraries
LIBS = 

$(TARGET):	$(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)

# MAKE
# --------------------------------------------------------------------------- .
all:	
	$(MAKE) $(TARGET)

# MAKE CLEAN
# --------------------------------------------------------------------------- .
clean:
	rm -f $(OBJS)
	
# MAKE CLEANALL
# --------------------------------------------------------------------------- .
cleanall:
	rm -f $(OBJS) $(TARGET)

run :
	$(TARGET)
