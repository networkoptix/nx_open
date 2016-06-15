 ###########################################################################
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
 ## --------------------------------------------------------------------  ##
 ## Common definitions for JDK versions                                   ##
 ##                                                                       ##
 ###########################################################################

ifndef JAVA_HOME
    JAVA_HOME = $(DEFAULT_JAVA_HOME)
endif

export JAVA_HOME

ifndef JMF_HOME
    JMF_HOME = $(DEFAULT_JMF_HOME)
endif

export JMF_HOME

ifndef JSAPI_HOME
    JSAPI_HOME = $(DEFAULT_JSAPI_HOME)
endif

export JSAPI_HOME

ifdef JMF_HOME
    SYSTEM_JAVA_MEDIA_CLASSPATH = $(JMF_HOME)/lib/jmf.jar
    SYSTEM_JAVA_MEDIA_LIBRARY = $(JMF_HOME)/lib
else
    SYSTEM_JAVA_MEDIA_CLASSPATH =
    SYSTEM_JAVA_MEDIA_LIBRARY = 
endif

ifndef JAVA_INCLUDES
    JAVA_INCLUDES  = -I$(JAVA_HOME)/include $(JAVA_SYSTEM_INCLUDES)
endif

ifndef JAVAC
    JAVAC       = javac
endif
ifndef JAVA
    JAVA       = java
endif
ifndef DEBUG_JAVAC
    DEBUG_JAVAC = $(JAVAC)
endif

ifndef JAVAH
    JAVAH       = javah -jni
endif
ifndef DEBUG_JAVAH
    DEBUG_JAVAH = $(JAVAH)
endif

ifndef JAR
    JAR         = jar cf0v
endif

