#########################################################-*-mode:Makefile-*-
 ##                                                                       ##
 ##                Centre for Speech Technology Research                  ##
 ##                     University of Edinburgh, UK                       ##
 ##                         Copyright (c) 1996                            ##
 ##                        All Rights Reserved.                           ##
 ##                                                                       ##
 ##  Permission is hereby granted, free of charge, to use and distribute  ##
 ##  this software and its documentation without restriction, including   ##
 ##  without limitation the rights to use, copy, modify, merge, publish,  ##
 ##  distribute, sublicense, and/or sell copies of this work, and to      ##
 ##  permit persons to whom this work is furnished to do so, subject to   ##
 ##  the following conditions:                                            ##
 ##   1. The code must retain the above copyright notice, this list of    ##
 ##      conditions and the following disclaimer.                         ##
 ##   2. Any modifications must be clearly marked as such.                ##
 ##   3. Original authors' names are not deleted.                         ##
 ##   4. The authors' names are not used to endorse or promote products   ##
 ##      derived from this software without specific prior written        ##
 ##      permission.                                                      ##
 ##                                                                       ##
 ##  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        ##
 ##  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      ##
 ##  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   ##
 ##  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     ##
 ##  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    ##
 ##  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   ##
 ##  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          ##
 ##  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       ##
 ##  THIS SOFTWARE.                                                       ##
 ##                                                                       ##
 ###########################################################################
 ##                                                                       ##
 ##                 Author: Richard Caley (rjc@cstr.ed.ac.uk)             ##
 ##                   Date: Wed Oct  1 1997                               ##
 ## --------------------------------------------------------------------  ##
 ## Things specific to the Speech Tools.                                  ##
 ##                                                                       ##
 ###########################################################################


PROJECT_NAME = Edinburgh Speech Tools Library
PROJECT_PREFIX = EST
PROJECT_VERSION = 2.4
PROJECT_DATE = December 2014
PROJECT_STATE = release

# Speech tools knows where speech_tools is. Probably.

EST=$(TOP)

# Where the main RCS tree is, probably only used within CSTR

CENTRAL_DIR = $(LOCAL_REPOSITORY)/speech_tools

# Place to find the optional modules for this project.

MODULE_DIRECTORY = $(TOP)

# List of all known modules

DISTRIBUTED_MODULES = \
	NAS_AUDIO ESD_AUDIO NATIVE_AUDIO MPLAYER_AUDIO \
	EDITLINE \
	SIOD WAGON SCFG WFST OLS \
	JAVA JAVA_CPP JAVA_MEDIA \
	TCL RXP

DEVELOPMENT_MODULES = \
	ASR 

UTILITY_MODULES = \
	EFENCE DMALLOC DEBUGGING

ALL_REAL_MODULES = \
	$(DISTRIBUTED_MODULES) \
	$(DEVELOPMENT_MODULES)

ALL_MODULES = \
	$(ALL_REAL_MODULES) \
	$(UTILITY_MODULES)

# Place where programs are compiled

PROJECT_MAIN_DIR=$(TOP)/main
PROJECT_SCRIPTS_DIR=$(TOP)/scripts
PROJECT_LIB_DIR = $(TOP)/lib


# Libraries defined in this project

PROJECT_LIBRARIES = estools estbase eststring 

PROJECT_LIBRARIES_JAVA = estjava

PROJECT_LIBRARY_DIR = lib
PROJECT_LIBRARY_DIR_estools = $(TOP)/lib
PROJECT_LIBRARY_DIR_estbase = $(TOP)/lib
PROJECT_LIBRARY_DIR_eststring = $(TOP)/lib
PROJECT_LIBRARY_DIR_estjava = $(TOP)/lib

PROJECT_LIBRARY_USES_estbase = eststring

PROJECT_LIBRARY_USES_estjava = estbase eststring

PROJECT_LIBRARY_VERSION_estools = $(PROJECT_VERSION).1
PROJECT_LIBRARY_VERSION_estbase = $(PROJECT_VERSION).1
PROJECT_LIBRARY_VERSION_eststring = 1.2
PROJECT_LIBRARY_VERSION_estjava = $(PROJECT_VERSION).1

PROJECT_LIBRARY_NEEDS_SYSLIBS_estjava=1

PROJECT_DEFAULT_LIBRARY = estools

PROJECT_SHARED_LIBRARIES = eststring estbase
PROJECT_ALL_LIBRARIES = eststring estbase estools

JAVA_CLASS_LIBRARY = $(LOCAL_JAVA_CLASS_LIBRARY)

JAVA_CLASSPATH=$(LOCAL_JAVA_CLASSPATH):$(SYSTEM_JAVA_CLASSPATH)

PROJECT_JAVA_ROOT=$(LOCAL_JAVA_ROOT)

# Libraries used from other projects

REQUIRED_LIBRARIES = 

# Includes for this and related projects

PROJECT_INCLUDES = -I$(TOP)/include

PROJECT_TEMPLATE_DIRS = include audio utils \
        base_class base_class/string base_class/templ_inst \
        ling_class speech_class sigpr stats grammar
PROJECT_TEMPLATE_DBS  = $(TOP)

# Places to look for documentation

DOCXX_DIRS = $(TOP)/include $(TOP)/testsuite

