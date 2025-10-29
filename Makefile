#-
#  ==========================================================================
#  Copyright 1995,2006,2008 Autodesk, Inc. All rights reserved.
# 
#  Use of this software is subject to the terms of the Autodesk
#  license agreement provided at the time of installation or download,
#  or which otherwise accompanies this software in either electronic
#  or hard copy form.
#  ==========================================================================
#+
NAME =	tcMotionPath
MAJOR =		1
MINOR =		0.0
VERSION =	$(MAJOR).$(MINOR)

LIBNAME =	$(NAME).so

C++			= g++
MAYAVERSION = 2025
# Maya 2025 requires C++17 and modern compiler
# Uncomment the following line on MIPSpro 7.2 compilers to supress
# warnings about declarations in the X header files
# WARNFLAGS	= -woff 3322

# When the following line is present, the linking of plugins will
# occur much faster.  If you link transitively (the default) then
# a number of "ld Warning 85" messages will be printed by ld.
# They are expected, and can be safely ignored.
MAYA_LOCATION = /usr/autodesk/maya$(MAYAVERSION)
INSTALL_PATH = /home/alan/projects/deploy/maya/tcMotionPath/1.0/plug-ins/$(MAYAVERSION)/linux
CFLAGS		= -std=c++17 -m64 -pthread -pipe -D_BOOL -DLINUX -DREQUIRE_IOSTREAM -DNOMINMAX -Wno-deprecated -fno-gnu-keywords -fPIC -DUSE_PTHREADS -DUSE_LICENSE -O2
C++FLAGS	= $(CFLAGS) $(WARNFLAGS)
INCLUDES	= -I. -I$(MAYA_LOCATION)/include -I./include -I/home/alan/projects/build/include/libxml2 -I/home/alan/projects/build/include -I/home/alan/projects/build/include/LicenseClient
LD			= $(C++) -shared $(C++FLAGS)
LIBS		= -L$(MAYA_LOCATION)/lib -lOpenMaya -lOpenMayaAnim -lOpenMayaRender -lOpenMayaUI -L/home/alan/projects/build/lib -lLicenseClient -lSockets -lssl -lcrypto -lpthread -ldl

.SUFFIXES: .cpp .cc .o .so .c

.cc.o:
	$(C++) -c $(INCLUDES) $(C++FLAGS) $<

.cpp.o:
	$(C++) -c $(INCLUDES) $(C++FLAGS) $< -o $@

.cc.i:
	$(C++) -E $(INCLUDES) $(C++FLAGS) $*.cc > $*.i

.cc.so:
	-rm -f $@
	$(LD) -o $@ $(INCLUDES) $< $(LIBS)

.cpp.so:
	-rm -f $@
	$(LD) -o $@ $(INCLUDES) $< $(LIBS)

.o.so:
	-rm -f $@
	$(LD) -o $@ $< $(LIBS)

CPP_FILES := $(wildcard source/*.cpp)
OBJS = $(addprefix source/,$(notdir $(CPP_FILES:.cpp=.o)))

all: $(LIBNAME)
	@echo "Done"
	mv $(LIBNAME) $(INSTALL_PATH)

$(LIBNAME): $(OBJS)
	-rm -f $@
	$(LD) -o $@ $(OBJS) $(LIBS) 

depend:
	makedepend $(INCLUDES) -I/usr/include/CC *.cc

clean:
	-rm -f source/*.o *.so

Clean:
	-rm -f source/*.o *.so *.bak
	
install:	all 
	mv $(LIBNAME) $(INSTALL_PATH)



